// SPDX-FileCopyrightText: 2025-2026 Uncore <https://github.com/uncor3>
// SPDX-License-Identifier: AGPL-3.0-or-later

use crate::{
    RUNTIME,
    companion_protocol::{COMPANION_PORT, GalleryEvent, GalleryImportEventKind, Session},
    device_ctx,
    qt_threading::{QtThread, QtThreading},
    qvariantmap_insert, settings_manager, utils,
};
use anyhow::{Context, Result, anyhow, bail};
use chrono::{DateTime, Utc};
use idevice::afc::{AfcClient, opcode::AfcFopenMode};
use log::{debug, error, info};
use macros::QtThreading;
use qmetaobject::prelude::*;
use qttypes::{QStringList, QVariantMap};
use serde_json::json;
use sha2::{Digest, Sha256};
use std::{
    path::{Path, PathBuf},
    sync::{
        Arc,
        atomic::{AtomicBool, Ordering},
    },
};
use tokio::{fs, io::AsyncReadExt, io::AsyncWriteExt, task::JoinHandle};
use uuid::Uuid;

const COMPANION_BUNDLE_ID: &str = "com.idescriptor.companion";
const AFC_IMPORT_ROOT: &str = "/Documents/Imports";
const CHUNK_SIZE: usize = 1024 * 1024;

#[allow(non_snake_case)]
#[derive(QObject, Default, QtThreading)]
pub struct WiredGalleryImport {
    base: qt_base_class!(trait QObject),
    state: qt_property!(QVariantMap; NOTIFY state_changed),
    state_changed: qt_signal!(),
    uploadProgress: qt_signal!(file_name: QString, bytes_uploaded: i64, total_bytes: i64),
    importEvent: qt_signal!(kind: i32, message: QString),
    errorOccurred: qt_signal!(message: QString),
    start: qt_method!(fn(&mut self, udid: QString, connection_id: i64, files: QStringList)),
    reconnect: qt_method!(fn(&mut self)),
    cancel: qt_method!(fn(&mut self)),
    task: Option<JoinHandle<()>>,
    cancel_flag: Arc<AtomicBool>,
    last_udid: String,
    last_connection_id: u64,
    last_batch_id: Option<Uuid>,
}

impl WiredGalleryImport {
    pub fn new_with_state() -> Self {
        let mut value = Self::default();
        value.state = make_state(
            "idle",
            "Choose photos and videos to import.",
            false,
            false,
            0,
            0,
            0,
        );
        value
    }

    fn start(&mut self, udid: QString, connection_id: i64, files: QStringList) {
        self.stop_task();

        let paths: Vec<PathBuf> = files
            .into_iter()
            .map(|path| PathBuf::from(path.to_string()))
            .filter(|path| path.is_file())
            .collect();
        if paths.is_empty() {
            self.replace_state(
                "error",
                "No gallery-compatible files selected.",
                false,
                false,
                0,
                0,
                0,
            );
            return;
        }
        if paths.len() > 500 {
            self.replace_state(
                "error",
                "A wired import can contain at most 500 items.",
                false,
                false,
                0,
                0,
                paths.len() as i64,
            );
            return;
        }
        if connection_id < 0 {
            self.replace_state(
                "error",
                "The selected device connection is invalid.",
                false,
                false,
                0,
                0,
                0,
            );
            return;
        }
        if let Some(path) = paths.iter().find(|path| media_info(path).is_none()) {
            self.replace_state(
                "error",
                &format!("Unsupported media file: {}", display_name(path)),
                false,
                false,
                0,
                0,
                paths.len() as i64,
            );
            return;
        }

        let udid = udid.to_string();
        let connection_id = connection_id as u64;
        let batch_id = Uuid::new_v4();
        let instance_id = match Uuid::parse_str(&settings_manager::companion_client_instance_id()) {
            Ok(value) => value,
            Err(error) => {
                self.replace_state("error", &error.to_string(), false, false, 0, 0, 0);
                return;
            }
        };

        self.last_udid = udid.clone();
        self.last_connection_id = connection_id;
        self.last_batch_id = Some(batch_id);
        self.cancel_flag = Arc::new(AtomicBool::new(false));
        let cancel = self.cancel_flag.clone();
        let qt = self.qt_thread();
        let total_items = paths.len() as i64;
        self.replace_state(
            "uploading",
            "Preparing the companion import…",
            true,
            true,
            0,
            0,
            total_items,
        );

        self.task = Some(RUNTIME.spawn(async move {
            let result = upload_batch(
                &udid,
                connection_id,
                batch_id,
                instance_id,
                paths,
                cancel.clone(),
                qt.clone(),
            )
            .await;

            match result {
                Ok(()) if cancel.load(Ordering::Relaxed) => {
                    set_state(
                        &qt,
                        "cancelled",
                        "Upload cancelled.",
                        false,
                        false,
                        0,
                        0,
                        total_items,
                    );
                }
                Ok(()) => {
                    listen_for_import(&udid, connection_id, batch_id, instance_id, qt.clone())
                        .await;
                }
                Err(error) => {
                    error!(
                        "Wired gallery upload failed: udid={} connection_id={} batch_id={} error={error:#}",
                        udid, connection_id, batch_id
                    );
                    cleanup_failed_upload(&udid, connection_id, batch_id).await;
                    emit_error(&qt, format!("{error:#}"));
                    set_state(
                        &qt,
                        "error",
                        &format!("{error:#}"),
                        false,
                        false,
                        0,
                        0,
                        total_items,
                    );
                }
            }
        }));
    }

    fn reconnect(&mut self) {
        let Some(batch_id) = self.last_batch_id else {
            self.replace_state(
                "error",
                "There is no import to reconnect.",
                false,
                false,
                0,
                0,
                0,
            );
            return;
        };
        self.stop_task();
        let instance_id = match Uuid::parse_str(&settings_manager::companion_client_instance_id()) {
            Ok(value) => value,
            Err(error) => {
                self.replace_state("error", &error.to_string(), false, false, 0, 0, 0);
                return;
            }
        };
        let udid = self.last_udid.clone();
        let connection_id = self.last_connection_id;
        let qt = self.qt_thread();
        self.replace_state(
            "connecting",
            "Connecting to iDescriptor Companion…",
            true,
            false,
            0,
            0,
            0,
        );
        self.task = Some(RUNTIME.spawn(async move {
            listen_for_import(&udid, connection_id, batch_id, instance_id, qt).await;
        }));
    }

    fn cancel(&mut self) {
        self.cancel_flag.store(true, Ordering::Relaxed);
        self.replace_state("cancelling", "Stopping the upload…", true, false, 0, 0, 0);
    }

    fn replace_state(
        &mut self,
        phase: &str,
        detail: &str,
        running: bool,
        can_cancel: bool,
        completed: i64,
        failed: i64,
        total: i64,
    ) {
        self.state = make_state(phase, detail, running, can_cancel, completed, failed, total);
        self.state_changed();
    }

    fn stop_task(&mut self) {
        if let Some(task) = self.task.take() {
            task.abort();
        }
    }
}

async fn cleanup_failed_upload(udid: &str, connection_id: u64, batch_id: Uuid) {
    let Some(services) = device_ctx::get_device_for_connection_opt(udid, connection_id).await
    else {
        return;
    };
    let provider = services.provider.lock().await;
    let Ok(mut afc) = utils::vend_app_documents(provider.as_ref(), COMPANION_BUNDLE_ID).await
    else {
        return;
    };
    drop(provider);
    let _ = afc
        .remove_all(format!("{AFC_IMPORT_ROOT}/Payload/{batch_id}"))
        .await;
    let _ = afc
        .remove_all(format!("{AFC_IMPORT_ROOT}/Queue/{batch_id}.json.part"))
        .await;
}

impl Drop for WiredGalleryImport {
    fn drop(&mut self) {
        self.stop_task();
    }
}

#[derive(Clone)]
struct UploadItem {
    id: Uuid,
    local_path: PathBuf,
    original_filename: String,
    extension: String,
    media_kind: &'static str,
    content_type: &'static str,
    byte_length: u64,
    creation_date: Option<String>,
}

async fn upload_batch(
    udid: &str,
    connection_id: u64,
    batch_id: Uuid,
    instance_id: Uuid,
    paths: Vec<PathBuf>,
    cancel: Arc<AtomicBool>,
    qt: QtThread<WiredGalleryImport>,
) -> Result<()> {
    let services = device_ctx::get_device_for_connection_opt(udid, connection_id)
        .await
        .ok_or_else(|| anyhow!("The selected device disconnected."))?;
    let mut items = Vec::with_capacity(paths.len());
    for path in paths {
        let (extension, media_kind, content_type) = media_info(&path)
            .ok_or_else(|| anyhow!("Unsupported media file: {}", display_name(&path)))?;
        let metadata = fs::metadata(&path)
            .await
            .with_context(|| format!("Failed to inspect {}", display_name(&path)))?;
        items.push(UploadItem {
            id: Uuid::new_v4(),
            original_filename: display_name(&path),
            extension,
            media_kind,
            content_type,
            byte_length: metadata.len(),
            creation_date: metadata
                .modified()
                .ok()
                .map(DateTime::<Utc>::from)
                .map(|date| date.format("%Y-%m-%dT%H:%M:%SZ").to_string()),
            local_path: path,
        });
    }

    let provider = services.provider.lock().await;
    let mut afc = utils::vend_app_documents(provider.as_ref(), COMPANION_BUNDLE_ID)
        .await
        .context("Could not open iDescriptor Companion shared storage. Make sure the app is installed and the iPhone is unlocked")?;
    drop(provider);

    let payload_parent = format!("{AFC_IMPORT_ROOT}/Payload");
    let queue_root = format!("{AFC_IMPORT_ROOT}/Queue");
    let processing_root = format!("{AFC_IMPORT_ROOT}/Processing");
    let payload_dir = format!("{payload_parent}/{batch_id}");
    ensure_dir(&mut afc, AFC_IMPORT_ROOT).await?;
    ensure_dir(&mut afc, &payload_parent).await?;
    ensure_dir(&mut afc, &queue_root).await?;
    ensure_dir(&mut afc, &processing_root).await?;
    ensure_dir(&mut afc, &payload_dir)
        .await
        .context("Failed to create the companion staging directory")?;
    info!(
        "Wired gallery staging ready: batch_id={} staging={}",
        batch_id, payload_dir
    );

    let total_bytes = items.iter().map(|item| item.byte_length).sum::<u64>();
    let mut batch_uploaded = 0_u64;
    let mut manifest_items = Vec::with_capacity(items.len());

    for item in &items {
        if cancel.load(Ordering::Relaxed) {
            let _ = afc.remove_all(&payload_dir).await;
            return Ok(());
        }
        let final_name = format!("{}.{}", item.id, item.extension);
        let remote_final = format!("{payload_dir}/{final_name}");
        let remote_part = format!("{remote_final}.part");
        let mut local = fs::File::open(&item.local_path)
            .await
            .with_context(|| format!("Failed to open {}", item.original_filename))?;
        let mut remote = afc
            .open(&remote_part, AfcFopenMode::WrOnly)
            .await
            .with_context(|| format!("Failed to stage {}", item.original_filename))?;
        let mut hasher = Sha256::new();
        let mut uploaded = 0_u64;
        let mut buffer = vec![0_u8; CHUNK_SIZE];

        loop {
            if cancel.load(Ordering::Relaxed) {
                break;
            }
            let count = local.read(&mut buffer).await?;
            if count == 0 {
                break;
            }
            hasher.update(&buffer[..count]);
            remote.write_all(&buffer[..count]).await?;
            uploaded += count as u64;
            let aggregate = batch_uploaded + uploaded;
            let filename = item.original_filename.clone();
            qt.queue(move |backend| {
                backend.uploadProgress(
                    QString::from(filename),
                    aggregate as i64,
                    total_bytes as i64,
                );
            });
        }
        remote.close().await?;
        if cancel.load(Ordering::Relaxed) {
            let _ = afc.remove_all(&payload_dir).await;
            return Ok(());
        }
        if uploaded != item.byte_length {
            let _ = afc.remove_all(&payload_dir).await;
            bail!(
                "Upload size changed while reading {}",
                item.original_filename
            );
        }
        afc.rename(&remote_part, &remote_final).await?;
        batch_uploaded += uploaded;
        manifest_items.push(json!({
            "id": item.id.to_string(),
            "relativePath": format!("Payload/{batch_id}/{final_name}"),
            "originalFilename": item.original_filename,
            "mediaKind": item.media_kind,
            "contentType": item.content_type,
            "byteLength": item.byte_length,
            "sha256": hex::encode(hasher.finalize()),
            "creationDate": item.creation_date,
        }));
    }

    let manifest = json!({
        "schemaVersion": 1,
        "batchID": batch_id.to_string(),
        "createdAt": Utc::now().format("%Y-%m-%dT%H:%M:%SZ").to_string(),
        "source": {
            "clientInstanceID": instance_id.to_string(),
            "deliveryMethod": 1,
        },
        "itemCount": manifest_items.len(),
        "totalBytes": total_bytes,
        "checksumAlgorithm": "sha256",
        "items": manifest_items,
    });
    let manifest_data = serde_json::to_vec_pretty(&manifest)?;
    let manifest_part = format!("{queue_root}/{batch_id}.json.part");
    let manifest_final = format!("{queue_root}/{batch_id}.json");
    let mut remote = afc.open(&manifest_part, AfcFopenMode::WrOnly).await?;
    remote.write_all(&manifest_data).await?;
    remote.close().await?;
    afc.rename(&manifest_part, &manifest_final).await?;

    set_state(
        &qt,
        "connecting",
        "Transfer complete. Connecting to iDescriptor Companion…",
        true,
        false,
        0,
        0,
        items.len() as i64,
    );
    Ok(())
}

async fn listen_for_import(
    udid: &str,
    connection_id: u64,
    batch_id: Uuid,
    instance_id: Uuid,
    qt: QtThread<WiredGalleryImport>,
) {
    let result: Result<()> = async {
        let services = device_ctx::get_device_for_connection_opt(udid, connection_id)
            .await
            .ok_or_else(|| anyhow!("The selected device disconnected."))?;
        let provider = services.provider.lock().await;
        let stream = provider
            .connect(COMPANION_PORT)
            .await
            .context("Open iDescriptor Companion on the iPhone, then choose Reconnect")?;
        drop(provider);
        let mut session = Session::new(stream);
        session.send_hello(instance_id).await?;
        set_state(
            &qt,
            "waiting",
            "Waiting for the companion to begin importing…",
            true,
            false,
            0,
            0,
            0,
        );

        loop {
            let event = session.next_gallery_event(batch_id, instance_id).await?;
            apply_event(&qt, &event);
            if event.kind.is_terminal() {
                if let Some(event_batch) = event.batch_id {
                    session.acknowledge(event_batch, event.revision).await?;
                }
                return Ok(());
            }
        }
    }
    .await;

    if let Err(error) = result {
        error!(
            "Wired gallery event connection failed: udid={} connection_id={} batch_id={} error={error:#}",
            udid, connection_id, batch_id
        );
        emit_error(&qt, format!("{error:#}"));
        set_state(
            &qt,
            "connectionRequired",
            &format!("{error:#}"),
            false,
            false,
            0,
            0,
            0,
        );
    }
}

fn emit_error(qt: &QtThread<WiredGalleryImport>, message: String) {
    qt.queue(move |backend| backend.errorOccurred(QString::from(message)));
}

fn apply_event(qt: &QtThread<WiredGalleryImport>, event: &GalleryEvent) {
    let (phase, default_detail, running) = match event.kind {
        GalleryImportEventKind::ImportServiceReady => (
            "waiting",
            "Companion is ready. Waiting for this batch…",
            true,
        ),
        GalleryImportEventKind::BatchNeedsAuthorization => (
            "authorization",
            "Allow Photos access in iDescriptor Companion to continue.",
            true,
        ),
        GalleryImportEventKind::BatchStarted => (
            "importing",
            "Companion started importing into Photos.",
            true,
        ),
        GalleryImportEventKind::ItemSucceeded => (
            "importing",
            "An item was added to Photos and the iDescriptor album.",
            true,
        ),
        GalleryImportEventKind::ItemFailed => ("importing", "An item could not be imported.", true),
        GalleryImportEventKind::BatchSucceeded => (
            "completed",
            "Import complete. The staged originals were released by Companion.",
            false,
        ),
        GalleryImportEventKind::BatchPartiallySucceeded => (
            "partial",
            "Import finished, but one or more items failed.",
            false,
        ),
        GalleryImportEventKind::BatchFailed => (
            "failed",
            "The companion could not import this batch.",
            false,
        ),
        GalleryImportEventKind::BatchCancelled => {
            ("cancelled", "The companion cancelled this import.", false)
        }
    };
    let detail = event
        .message
        .clone()
        .unwrap_or_else(|| default_detail.to_string());
    let kind = event.kind as i32;
    let completed = event.completed_items as i64;
    let failed = event.failed_items as i64;
    let total = event.total_items as i64;
    qt.queue(move |backend| {
        backend.state = make_state(phase, &detail, running, false, completed, failed, total);
        backend.state_changed();
        backend.importEvent(kind, QString::from(detail));
    });
}

fn set_state(
    qt: &QtThread<WiredGalleryImport>,
    phase: &'static str,
    detail: &str,
    running: bool,
    can_cancel: bool,
    completed: i64,
    failed: i64,
    total: i64,
) {
    let detail = detail.to_string();
    qt.queue(move |backend| {
        backend.state = make_state(
            phase, &detail, running, can_cancel, completed, failed, total,
        );
        backend.state_changed();
    });
}

fn make_state(
    phase: &str,
    detail: &str,
    running: bool,
    can_cancel: bool,
    completed: i64,
    failed: i64,
    total: i64,
) -> QVariantMap {
    let mut state = QVariantMap::default();
    qvariantmap_insert!(state, "phase", QString::from(phase));
    qvariantmap_insert!(state, "detail", QString::from(detail));
    qvariantmap_insert!(state, "running", running);
    qvariantmap_insert!(state, "canCancel", can_cancel);
    qvariantmap_insert!(state, "completedItems", completed);
    qvariantmap_insert!(state, "failedItems", failed);
    qvariantmap_insert!(state, "totalItems", total);
    state
}

async fn ensure_dir(afc: &mut AfcClient, path: &str) -> Result<()> {
    match afc.mk_dir(path).await {
        Ok(()) => {
            debug!("Created Companion AFC directory: {path}");
            Ok(())
        }
        Err(create_error) => match afc.list_dir(path).await {
            Ok(_) => {
                debug!("Companion AFC directory already exists: {path}");
                Ok(())
            }
            Err(inspect_error) => {
                error!(
                    "Failed to create Companion AFC directory: path={} create_error={} inspect_error={}",
                    path, create_error, inspect_error
                );
                Err(anyhow!(
                    "Failed to create Companion directory {path:?}: {create_error} (verification failed: {inspect_error})"
                ))
            }
        },
    }
}

fn display_name(path: &Path) -> String {
    path.file_name()
        .and_then(|name| name.to_str())
        .unwrap_or("media")
        .to_string()
}

fn media_info(path: &Path) -> Option<(String, &'static str, &'static str)> {
    let extension = path.extension()?.to_str()?.to_ascii_lowercase();
    let (kind, content_type) = match extension.as_str() {
        "jpg" | "jpeg" => ("photo", "public.jpeg"),
        "png" => ("photo", "public.png"),
        "heic" => ("photo", "public.heic"),
        "heif" => ("photo", "public.heif"),
        "gif" => ("photo", "com.compuserve.gif"),
        "tif" | "tiff" => ("photo", "public.tiff"),
        "bmp" => ("photo", "com.microsoft.bmp"),
        "dng" => ("photo", "com.adobe.raw-image"),
        "mov" => ("video", "com.apple.quicktime-movie"),
        "mp4" | "m4v" => ("video", "public.mpeg-4"),
        "3gp" => ("video", "public.3gpp"),
        _ => return None,
    };
    Some((extension, kind, content_type))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn media_extensions_are_normalized_and_conservative() {
        assert_eq!(media_info(Path::new("Photo.JPEG")).unwrap().0, "jpeg");
        assert_eq!(media_info(Path::new("clip.MOV")).unwrap().1, "video");
        assert!(media_info(Path::new("archive.zip")).is_none());
        assert!(media_info(Path::new("movie.mkv")).is_none());
    }
}
