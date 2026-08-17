// SPDX-FileCopyrightText: 2025-2026 Uncore <https://github.com/uncor3>
// SPDX-License-Identifier: AGPL-3.0-or-later

use crate::{
    RUNTIME, device_ctx,
    qt_threading::{QtThread, QtThreading},
    utils,
};
use idevice::IdeviceService;
use idevice::afc::{AfcClient, FileInfo, opcode::AfcFopenMode};
use log::{debug, error, info, warn};
use macros::QtThreading;
use qmetaobject::prelude::*;
use qttypes::QStringList;
use std::{
    collections::{HashMap, HashSet},
    path::{Path, PathBuf},
    sync::{
        Arc, Mutex,
        atomic::{AtomicBool, Ordering},
    },
};
use tokio::{fs, io::AsyncWriteExt};

pub static DEFAULT_CHUNK_SIZE: usize = 1024 * 1024;

#[derive(QObject, Default, QtThreading)]
#[allow(non_snake_case)]
pub struct IOManager {
    base: qt_base_class!(trait QObject),
    jobs: Arc<Mutex<HashMap<String, Arc<AtomicBool>>>>,
    start_export: qt_method!(
        fn(
            &self,
            udid: QString,
            job_id: QString,
            device_paths: QStringList,
            destination_dir: QString,
            allow_directories: bool,
        )
    ),
    start_export_with_afc2: qt_method!(
        fn(
            &self,
            udid: QString,
            job_id: QString,
            device_paths: QStringList,
            destination_dir: QString,
            allow_directories: bool,
        )
    ),
    start_export_with_hause_arrest_afc: qt_method!(
        fn(
            &self,
            udid: QString,
            job_id: QString,
            device_paths: QStringList,
            destination_dir: QString,
            hause_arrest_afc: QString,
            allow_directories: bool,
        )
    ),
    start_import: qt_method!(
        fn(
            &self,
            udid: QString,
            job_id: QString,
            local_paths: QStringList,
            destination_dir: QString,
        )
    ),
    start_import_with_afc2: qt_method!(
        fn(
            &self,
            udid: QString,
            job_id: QString,
            local_paths: QStringList,
            destination_dir: QString,
        )
    ),
    start_import_with_hause_arrest_afc: qt_method!(
        fn(
            &self,
            udid: QString,
            job_id: QString,
            local_paths: QStringList,
            destination_dir: QString,
            hause_arrest_afc: QString,
        )
    ),
    has_active_tasks: qt_method!(fn(&self) -> bool),
    cancel_job: qt_method!(fn(&self, job_id: QString)),
    cancel_all_jobs: qt_method!(fn(&self)),
    fileTransferProgress: qt_signal!(
        job_id: QString,
        file_name: QString,
        bytes_transferred: i64,
        total_bytes: i64
    ),
    exportItemFinished: qt_signal!(
        job_id: QString,
        file_name: QString,
        destination_path: QString,
        success: bool,
        bytes_transferred: i64,
        error_message: QString
    ),
    exportJobFinished: qt_signal!(
        job_id: QString,
        cancelled: bool,
        successful_items: i32,
        failed_items: i32,
        total_bytes: i64
    ),
    importItemFinished: qt_signal!(
        job_id: QString,
        file_name: QString,
        destination_path: QString,
        success: bool,
        bytes_transferred: i64,
        error_message: QString
    ),
    importJobFinished: qt_signal!(
        job_id: QString,
        cancelled: bool,
        successful_items: i32,
        failed_items: i32,
        total_bytes: i64
    ),
}

struct TransferItemResult {
    success: bool,
    bytes_transferred: i64,
    destination_path: String,
    error_message: Option<String>,
}

struct RemoteExportFile {
    remote_path: String,
    relative_path: PathBuf,
    info: FileInfo,
}

struct DirectoryExportManifest {
    directories: Vec<PathBuf>,
    files: Vec<RemoteExportFile>,
    total_bytes: i64,
}

enum AfcKind {
    Standard,
    Afc2,
    HouseArrest(String),
}

impl AfcKind {
    fn description(&self) -> String {
        match self {
            AfcKind::Standard => "standard".to_string(),
            AfcKind::Afc2 => "afc2".to_string(),
            AfcKind::HouseArrest(bundle_id) => format!("house-arrest:{bundle_id}"),
        }
    }
}

impl IOManager {
    fn has_active_tasks(&self) -> bool {
        !self
            .jobs
            .lock()
            .expect("IOManager jobs map mutex poisoned")
            .is_empty()
    }

    fn start_export(
        &self,
        udid: QString,
        job_id: QString,
        device_paths: QStringList,
        destination_dir: QString,
        allow_directories: bool,
    ) {
        self.spawn_export(
            udid,
            job_id,
            device_paths,
            destination_dir,
            AfcKind::Standard,
            allow_directories,
        );
    }

    fn start_export_with_afc2(
        &self,
        udid: QString,
        job_id: QString,
        device_paths: QStringList,
        destination_dir: QString,
        allow_directories: bool,
    ) {
        self.spawn_export(
            udid,
            job_id,
            device_paths,
            destination_dir,
            AfcKind::Afc2,
            allow_directories,
        );
    }

    fn start_export_with_hause_arrest_afc(
        &self,
        udid: QString,
        job_id: QString,
        device_paths: QStringList,
        destination_dir: QString,
        hause_arrest_afc: QString,
        allow_directories: bool,
    ) {
        self.spawn_export(
            udid,
            job_id,
            device_paths,
            destination_dir,
            AfcKind::HouseArrest(hause_arrest_afc.to_string()),
            allow_directories,
        );
    }

    fn start_import(
        &self,
        udid: QString,
        job_id: QString,
        local_paths: QStringList,
        destination_dir: QString,
    ) {
        self.spawn_import(
            udid,
            job_id,
            local_paths,
            destination_dir,
            AfcKind::Standard,
        );
    }

    fn start_import_with_afc2(
        &self,
        udid: QString,
        job_id: QString,
        local_paths: QStringList,
        destination_dir: QString,
    ) {
        self.spawn_import(udid, job_id, local_paths, destination_dir, AfcKind::Afc2);
    }

    fn start_import_with_hause_arrest_afc(
        &self,
        udid: QString,
        job_id: QString,
        local_paths: QStringList,
        destination_dir: QString,
        hause_arrest_afc: QString,
    ) {
        self.spawn_import(
            udid,
            job_id,
            local_paths,
            destination_dir,
            AfcKind::HouseArrest(hause_arrest_afc.to_string()),
        );
    }

    fn cancel_job(&self, job_id: QString) {
        let job_id_str = job_id.to_string();
        let guard = self.jobs.lock().expect("IOManager jobs map mutex poisoned");
        if let Some(flag) = guard.get(&job_id_str) {
            info!("IOManager cancel requested: job_id={job_id_str}");
            flag.store(true, Ordering::Relaxed);
        } else {
            warn!("IOManager cancel requested for unknown job: job_id={job_id_str}");
        }
    }

    fn cancel_all_jobs(&self) {
        let guard = self.jobs.lock().expect("IOManager jobs map mutex poisoned");
        info!("IOManager cancel all requested: jobs={}", guard.len());
        for flag in guard.values() {
            flag.store(true, Ordering::Relaxed);
        }
    }

    fn spawn_export(
        &self,
        udid: QString,
        job_id: QString,
        device_paths: QStringList,
        destination_dir: QString,
        afc_kind: AfcKind,
        allow_directories: bool,
    ) {
        let udid = udid.to_string();
        let job_id = job_id.to_string();
        let destination_dir = destination_dir.to_string();
        let items = qstring_list_to_vec(device_paths);
        let item_count = items.len();
        let afc_kind_description = afc_kind.description();
        info!(
            "IOManager export requested: job_id={job_id} udid={udid} items={item_count} destination_dir={destination_dir} afc={afc_kind_description}"
        );
        if item_count == 0 {
            warn!("IOManager export requested with no items: job_id={job_id}");
        }
        let cancel_flag = self.register_job(&job_id);
        let jobs = self.jobs.clone();
        let qt_thread = self.qt_thread();

        RUNTIME.spawn(async move {
            debug!(
                "IOManager export creating AFC client: job_id={job_id} udid={udid} afc={afc_kind_description}"
            );
            let mut afc = match create_afc_client(&udid, afc_kind).await {
                Ok(afc) => {
                    debug!("IOManager export AFC client ready: job_id={job_id}");
                    afc
                }
                Err(err) => {
                    error!(
                        "IOManager export failed to create AFC client: job_id={job_id} udid={udid} afc={afc_kind_description}: {err}"
                    );
                    finish_export_job(&qt_thread, job_id.clone(), true, 0, 0, 0);
                    unregister_job(&jobs, &job_id);
                    return;
                }
            };

            handle_start_export(
                &mut afc,
                job_id.clone(),
                items,
                destination_dir,
                qt_thread,
                jobs,
                cancel_flag,
                allow_directories,
            )
            .await;
        });
    }

    fn spawn_import(
        &self,
        udid: QString,
        job_id: QString,
        local_paths: QStringList,
        destination_dir: QString,
        afc_kind: AfcKind,
    ) {
        let udid = udid.to_string();
        let job_id = job_id.to_string();
        let destination_dir = destination_dir.to_string();
        let items = qstring_list_to_vec(local_paths);
        let item_count = items.len();
        let afc_kind_description = afc_kind.description();
        info!(
            "IOManager import requested: job_id={job_id} udid={udid} items={item_count} destination_dir={destination_dir} afc={afc_kind_description}"
        );
        if item_count == 0 {
            warn!("IOManager import requested with no items: job_id={job_id}");
        }
        let cancel_flag = self.register_job(&job_id);
        let jobs = self.jobs.clone();
        let qt_thread = self.qt_thread();

        RUNTIME.spawn(async move {
            debug!(
                "IOManager import creating AFC client: job_id={job_id} udid={udid} afc={afc_kind_description}"
            );
            let mut afc = match create_afc_client(&udid, afc_kind).await {
                Ok(afc) => {
                    debug!("IOManager import AFC client ready: job_id={job_id}");
                    afc
                }
                Err(err) => {
                    error!(
                        "IOManager import failed to create AFC client: job_id={job_id} udid={udid} afc={afc_kind_description}: {err}"
                    );
                    finish_import_job(&qt_thread, job_id.clone(), true, 0, 0, 0);
                    unregister_job(&jobs, &job_id);
                    return;
                }
            };

            handle_start_import(
                &mut afc,
                job_id.clone(),
                items,
                destination_dir,
                qt_thread,
                jobs,
                cancel_flag,
            )
            .await;
        });
    }

    fn register_job(&self, job_id: &str) -> Arc<AtomicBool> {
        let cancel_flag = Arc::new(AtomicBool::new(false));
        let mut guard = self.jobs.lock().expect("IOManager jobs map mutex poisoned");
        if guard
            .insert(job_id.to_string(), cancel_flag.clone())
            .is_some()
        {
            warn!("IOManager replaced existing job registration: job_id={job_id}");
        } else {
            debug!("IOManager registered job: job_id={job_id}");
        }
        cancel_flag
    }
}

fn qstring_list_to_vec(paths: QStringList) -> Vec<String> {
    paths.into_iter().map(|path| path.to_string()).collect()
}

async fn create_afc_client(udid: &str, afc_kind: AfcKind) -> anyhow::Result<AfcClient> {
    let afc_kind_description = afc_kind.description();
    debug!("IOManager resolving device for AFC client: udid={udid} afc={afc_kind_description}");
    let device = device_ctx::get_device_opt(udid)
        .await
        .ok_or_else(|| anyhow::anyhow!("device {udid} not found"))?;

    match afc_kind {
        AfcKind::Standard => Ok(AfcClient::connect(device.provider.lock().await.as_ref()).await?),
        AfcKind::Afc2 => Ok(AfcClient::new_afc2(device.provider.lock().await.as_ref()).await?),
        AfcKind::HouseArrest(bundle_id) => {
            let provider = device.provider.lock().await;
            Ok(utils::vend_app_documents(provider.as_ref(), &bundle_id).await?)
        }
    }
}

async fn handle_start_export(
    afc: &mut AfcClient,
    job_id: String,
    device_paths: Vec<String>,
    destination_dir: String,
    qt_thread: QtThread<IOManager>,
    jobs: Arc<Mutex<HashMap<String, Arc<AtomicBool>>>>,
    cancel_flag: Arc<AtomicBool>,
    allow_directories: bool,
) {
    let mut successful = 0_i32;
    let mut failed = 0_i32;
    let mut total_bytes = 0_i64;
    let mut cancelled = false;

    debug!(
        "IOManager export job started: job_id={job_id} items={}",
        device_paths.len()
    );

    for device_path in device_paths {
        if cancel_flag.load(Ordering::Relaxed) {
            cancelled = true;
            info!("IOManager export job cancellation observed: job_id={job_id}");
            break;
        }

        debug!("IOManager export item requested: job_id={job_id} device_path={device_path}");
        match export_single_item(
            afc,
            &device_path,
            &destination_dir,
            &job_id,
            &qt_thread,
            &cancel_flag,
            allow_directories,
        )
        .await
        {
            Ok(result) if result.success => {
                successful += 1;
                total_bytes += result.bytes_transferred;
                debug!(
                    "IOManager export item finished: job_id={job_id} device_path={device_path} bytes={}",
                    result.bytes_transferred
                );
                emit_export_item_finished(&qt_thread, &job_id, &device_path, result, true);
            }
            Ok(result) => {
                failed += 1;
                warn!(
                    "IOManager export item did not complete successfully: job_id={job_id} device_path={device_path} bytes={} error={}",
                    result.bytes_transferred,
                    result.error_message.as_deref().unwrap_or("cancelled")
                );
                emit_export_item_finished(&qt_thread, &job_id, &device_path, result, false);
            }
            Err(err) => {
                failed += 1;
                error!(
                    "IOManager export item failed: job_id={job_id} device_path={device_path}: {err}"
                );
                let file_name = file_name_for_path(&device_path);
                let job_id_signal = job_id.clone();
                qt_thread.queue(move |mgr| {
                    mgr.exportItemFinished(
                        QString::from(job_id_signal),
                        QString::from(file_name),
                        QString::default(),
                        false,
                        0,
                        QString::from(err),
                    );
                });
            }
        }
    }

    finish_export_job(
        &qt_thread,
        job_id.clone(),
        cancelled,
        successful,
        failed,
        total_bytes,
    );
    info!(
        "IOManager export job finished: job_id={job_id} cancelled={cancelled} successful={successful} failed={failed} total_bytes={total_bytes}"
    );
    unregister_job(&jobs, &job_id);
}

async fn handle_start_import(
    afc: &mut AfcClient,
    job_id: String,
    local_paths: Vec<String>,
    destination_dir: String,
    qt_thread: QtThread<IOManager>,
    jobs: Arc<Mutex<HashMap<String, Arc<AtomicBool>>>>,
    cancel_flag: Arc<AtomicBool>,
) {
    let mut successful = 0_i32;
    let mut failed = 0_i32;
    let mut total_bytes = 0_i64;
    let mut cancelled = false;

    debug!(
        "IOManager import job started: job_id={job_id} items={}",
        local_paths.len()
    );

    for local_path in local_paths {
        if cancel_flag.load(Ordering::Relaxed) {
            cancelled = true;
            info!("IOManager import job cancellation observed: job_id={job_id}");
            break;
        }

        debug!("IOManager import item requested: job_id={job_id} local_path={local_path}");
        match import_single_item(
            afc,
            &local_path,
            &destination_dir,
            &job_id,
            &qt_thread,
            &cancel_flag,
        )
        .await
        {
            Ok(result) if result.success => {
                successful += 1;
                total_bytes += result.bytes_transferred;
                debug!(
                    "IOManager import item finished: job_id={job_id} local_path={local_path} destination_path={} bytes={}",
                    result.destination_path, result.bytes_transferred
                );
                emit_import_item_finished(&qt_thread, &job_id, &local_path, result, true);
            }
            Ok(result) => {
                failed += 1;
                warn!(
                    "IOManager import item did not complete successfully: job_id={job_id} local_path={local_path} destination_path={} bytes={} error={}",
                    result.destination_path,
                    result.bytes_transferred,
                    result.error_message.as_deref().unwrap_or("cancelled")
                );
                emit_import_item_finished(&qt_thread, &job_id, &local_path, result, false);
            }
            Err(err) => {
                failed += 1;
                error!(
                    "IOManager import item failed: job_id={job_id} local_path={local_path}: {err}"
                );
                let file_name = file_name_for_path(&local_path);
                let job_id_signal = job_id.clone();
                qt_thread.queue(move |mgr| {
                    mgr.importItemFinished(
                        QString::from(job_id_signal),
                        QString::from(file_name),
                        QString::default(),
                        false,
                        0,
                        QString::from(err),
                    );
                });
            }
        }
    }

    finish_import_job(
        &qt_thread,
        job_id.clone(),
        cancelled,
        successful,
        failed,
        total_bytes,
    );
    info!(
        "IOManager import job finished: job_id={job_id} cancelled={cancelled} successful={successful} failed={failed} total_bytes={total_bytes}"
    );
    unregister_job(&jobs, &job_id);
}

async fn export_single_item(
    afc: &mut AfcClient,
    device_path: &str,
    destination_dir: &str,
    job_id: &str,
    qt_thread: &QtThread<IOManager>,
    cancel_flag: &Arc<AtomicBool>,
    allow_directories: bool,
) -> Result<TransferItemResult, String> {
    fs::create_dir_all(destination_dir)
        .await
        .map_err(|e| format!("Failed to create destination directory {destination_dir}: {e}"))?;

    let file_name = file_name_for_path(device_path);
    let base_path = Path::new(destination_dir).join(&file_name);
    let output_path = unique_output_path(&base_path).await;
    let output_path_str = output_path.to_string_lossy().to_string();
    debug!(
        "IOManager export item preparing: job_id={job_id} device_path={device_path} output_path={output_path_str}"
    );

    let resolved = afc
        .get_file_info_resolved(device_path.to_string())
        .await
        .map_err(|e| format!("Failed to resolve device path {device_path}: {e}"))?;
    let resolved_path = resolved.resolved_path;
    let info = resolved.info;

    if info.st_ifmt == "S_IFDIR" {
        if !allow_directories {
            return Err(format!("Directory export is not enabled for {device_path}"));
        }

        return export_directory(
            afc,
            &resolved_path,
            &output_path,
            &file_name,
            job_id,
            qt_thread,
            cancel_flag,
        )
        .await;
    }

    let transferred = export_remote_file(
        afc,
        &resolved_path,
        &output_path,
        &info,
        job_id,
        qt_thread,
        &file_name,
        0,
        info.size as i64,
        cancel_flag,
    )
    .await?;

    Ok(TransferItemResult {
        success: !cancel_flag.load(Ordering::Relaxed),
        bytes_transferred: transferred,
        destination_path: output_path_str,
        error_message: cancel_flag
            .load(Ordering::Relaxed)
            .then(|| "Export cancelled".to_string()),
    })
}

async fn export_directory(
    afc: &mut AfcClient,
    device_path: &str,
    output_path: &Path,
    progress_name: &str,
    job_id: &str,
    qt_thread: &QtThread<IOManager>,
    cancel_flag: &Arc<AtomicBool>,
) -> Result<TransferItemResult, String> {
    let manifest = build_directory_export_manifest(afc, device_path, cancel_flag).await?;
    let output_path_str = output_path.to_string_lossy().to_string();

    for relative_directory in &manifest.directories {
        if cancel_flag.load(Ordering::Relaxed) {
            return Ok(TransferItemResult {
                success: false,
                bytes_transferred: 0,
                destination_path: output_path_str,
                error_message: Some("Export cancelled".to_string()),
            });
        }

        let local_directory = output_path.join(relative_directory);
        fs::create_dir_all(&local_directory).await.map_err(|err| {
            format!(
                "Failed to create exported directory {}: {err}",
                local_directory.display()
            )
        })?;
    }

    let mut transferred = 0_i64;
    for file in manifest.files {
        if cancel_flag.load(Ordering::Relaxed) {
            break;
        }

        let local_path = output_path.join(&file.relative_path);
        if let Some(parent) = local_path.parent() {
            fs::create_dir_all(parent).await.map_err(|err| {
                format!(
                    "Failed to create exported directory {}: {err}",
                    parent.display()
                )
            })?;
        }

        transferred += export_remote_file(
            afc,
            &file.remote_path,
            &local_path,
            &file.info,
            job_id,
            qt_thread,
            progress_name,
            transferred,
            manifest.total_bytes,
            cancel_flag,
        )
        .await?;
    }

    Ok(TransferItemResult {
        success: !cancel_flag.load(Ordering::Relaxed),
        bytes_transferred: transferred,
        destination_path: output_path_str,
        error_message: cancel_flag
            .load(Ordering::Relaxed)
            .then(|| "Export cancelled".to_string()),
    })
}

async fn build_directory_export_manifest(
    afc: &mut AfcClient,
    device_path: &str,
    cancel_flag: &Arc<AtomicBool>,
) -> Result<DirectoryExportManifest, String> {
    let mut manifest = DirectoryExportManifest {
        directories: vec![PathBuf::new()],
        files: Vec::new(),
        total_bytes: 0,
    };
    let mut root_ancestors = HashSet::new();
    root_ancestors.insert(device_path.to_string());
    let mut pending_directories = vec![(device_path.to_string(), PathBuf::new(), root_ancestors)];

    while let Some((remote_directory, relative_directory, ancestors)) = pending_directories.pop() {
        if cancel_flag.load(Ordering::Relaxed) {
            return Err("Export cancelled while enumerating directory".to_string());
        }

        let entries = afc
            .list_dir(&remote_directory)
            .await
            .map_err(|err| format!("Failed to list directory {remote_directory}: {err}"))?;

        for name in entries {
            if name == "." || name == ".." {
                continue;
            }
            validate_remote_child_name(&name)?;

            let remote_path = remote_child_path(&remote_directory, &name);
            let relative_path = relative_directory.join(&name);
            let resolved = afc
                .get_file_info_resolved(&remote_path)
                .await
                .map_err(|err| format!("Failed to resolve {remote_path}: {err}"))?;
            let resolved_path = resolved.resolved_path;
            let info = resolved.info;

            if info.st_ifmt == "S_IFDIR" {
                if ancestors.contains(&resolved_path) {
                    return Err(format!(
                        "Refusing symbolic-link directory cycle at {remote_path} -> {resolved_path}"
                    ));
                }
                // only current ancestors are checked,
                // so separate aliases may export the same target
                // this should be ok
                let mut child_ancestors = ancestors.clone();
                child_ancestors.insert(resolved_path.clone());
                manifest.directories.push(relative_path.clone());
                pending_directories.push((resolved_path, relative_path, child_ancestors));
            } else {
                manifest.total_bytes = manifest.total_bytes.saturating_add(info.size as i64);
                manifest.files.push(RemoteExportFile {
                    remote_path: resolved_path,
                    relative_path,
                    info,
                });
            }
        }
    }

    Ok(manifest)
}

#[allow(clippy::too_many_arguments)]
async fn export_remote_file(
    afc: &mut AfcClient,
    device_path: &str,
    output_path: &Path,
    info: &FileInfo,
    job_id: &str,
    qt_thread: &QtThread<IOManager>,
    progress_name: &str,
    progress_offset: i64,
    progress_total: i64,
    cancel_flag: &Arc<AtomicBool>,
) -> Result<i64, String> {
    use tokio::io::AsyncReadExt;

    let output_path_str = output_path.to_string_lossy().to_string();
    let mut remote = afc
        .open(device_path, AfcFopenMode::RdOnly)
        .await
        .map_err(|err| format!("Failed to open device file {device_path}: {err}"))?;
    let mut local = fs::File::create(output_path)
        .await
        .map_err(|err| format!("Failed to create local file {output_path_str}: {err}"))?;
    let mut chunk = vec![0_u8; DEFAULT_CHUNK_SIZE];
    let mut transferred = 0_i64;

    loop {
        if cancel_flag.load(Ordering::Relaxed) {
            break;
        }

        let read = remote
            .read(&mut chunk)
            .await
            .map_err(|err| format!("Failed to read from device file {device_path}: {err}"))?;
        if read == 0 {
            break;
        }

        local
            .write_all(&chunk[..read])
            .await
            .map_err(|err| format!("Failed to write to local file {output_path_str}: {err}"))?;
        transferred += read as i64;
        emit_progress(
            qt_thread,
            job_id,
            progress_name,
            progress_offset.saturating_add(transferred),
            progress_total,
        );
    }

    let _ = remote.close().await;
    let _ = local.flush().await;

    if transferred > 0 {
        let modified_utc = info.modified.and_utc();
        let mtime = filetime::FileTime::from_unix_time(
            modified_utc.timestamp(),
            modified_utc.timestamp_subsec_nanos(),
        );
        if let Err(err) = filetime::set_file_times(output_path, mtime, mtime) {
            warn!("Failed to preserve file time for {output_path_str}: {err}");
        }
    }

    Ok(transferred)
}

fn remote_child_path(parent: &str, name: &str) -> String {
    if parent.ends_with('/') {
        format!("{parent}{name}")
    } else {
        format!("{parent}/{name}")
    }
}

fn validate_remote_child_name(name: &str) -> Result<(), String> {
    let mut components = Path::new(name).components();
    let valid = matches!(components.next(), Some(std::path::Component::Normal(_)))
        && components.next().is_none();
    if valid {
        Ok(())
    } else {
        Err(format!("Refusing unsafe remote directory entry {name:?}"))
    }
}

async fn import_single_item(
    afc: &mut AfcClient,
    local_path: &str,
    destination_dir: &str,
    job_id: &str,
    qt_thread: &QtThread<IOManager>,
    cancel_flag: &Arc<AtomicBool>,
) -> Result<TransferItemResult, String> {
    use tokio::io::AsyncReadExt;

    let file_name = file_name_for_path(local_path);
    let device_path = if destination_dir.ends_with('/') {
        format!("{destination_dir}{file_name}")
    } else {
        format!("{destination_dir}/{file_name}")
    };
    debug!(
        "IOManager import item preparing: job_id={job_id} local_path={local_path} device_path={device_path}"
    );

    let mut local = fs::File::open(local_path)
        .await
        .map_err(|e| format!("Failed to open local file {local_path}: {e}"))?;
    let metadata = local
        .metadata()
        .await
        .map_err(|e| format!("Failed to stat local file {local_path}: {e}"))?;
    let file_size = metadata.len() as i64;

    let mut remote = afc
        .open(&device_path, AfcFopenMode::WrOnly)
        .await
        .map_err(|e| format!("Failed to open device file {device_path} for writing: {e}"))?;

    let mut chunk = vec![0u8; DEFAULT_CHUNK_SIZE];
    let mut transferred = 0_i64;

    loop {
        if cancel_flag.load(Ordering::Relaxed) {
            debug!(
                "IOManager import item cancellation observed: job_id={job_id} local_path={local_path} bytes={transferred}"
            );
            break;
        }

        let read = local
            .read(&mut chunk)
            .await
            .map_err(|e| format!("Failed to read from local file {local_path}: {e}"))?;
        if read == 0 {
            break;
        }

        remote
            .write_all(&chunk[..read])
            .await
            .map_err(|e| format!("Failed to write to device file {device_path}: {e}"))?;
        transferred += read as i64;

        emit_progress(qt_thread, job_id, &file_name, transferred, file_size);
    }

    let _ = remote.close().await;

    Ok(TransferItemResult {
        success: !cancel_flag.load(Ordering::Relaxed),
        bytes_transferred: transferred,
        destination_path: device_path,
        error_message: None,
    })
}

async fn unique_output_path(base_path: &Path) -> PathBuf {
    if fs::try_exists(base_path).await.unwrap_or(false) {
        let stem = base_path
            .file_stem()
            .and_then(|stem| stem.to_str())
            .unwrap_or("file");
        let ext = base_path.extension().and_then(|ext| ext.to_str());
        let parent = base_path.parent().unwrap_or_else(|| Path::new("."));

        for index in 1..1000 {
            let file_name = match ext {
                Some(ext) if !ext.is_empty() => format!("{stem} ({index}).{ext}"),
                _ => format!("{stem} ({index})"),
            };
            let candidate = parent.join(file_name);
            if !fs::try_exists(&candidate).await.unwrap_or(false) {
                return candidate;
            }
        }
    }

    base_path.to_path_buf()
}

fn file_name_for_path(path: &str) -> String {
    Path::new(path)
        .file_name()
        .and_then(|name| name.to_str())
        .unwrap_or(path)
        .to_string()
}

fn emit_progress(
    qt_thread: &QtThread<IOManager>,
    job_id: &str,
    file_name: &str,
    transferred: i64,
    total: i64,
) {
    let job_id = job_id.to_string();
    let file_name = file_name.to_string();
    qt_thread.queue(move |mgr| {
        mgr.fileTransferProgress(
            QString::from(job_id),
            QString::from(file_name),
            transferred,
            total,
        );
    });
}

fn emit_export_item_finished(
    qt_thread: &QtThread<IOManager>,
    job_id: &str,
    source_path: &str,
    result: TransferItemResult,
    success: bool,
) {
    let job_id = job_id.to_string();
    let file_name = file_name_for_path(source_path);
    let destination_path = result.destination_path;
    let error_message = result.error_message.unwrap_or_default();
    qt_thread.queue(move |mgr| {
        mgr.exportItemFinished(
            QString::from(job_id),
            QString::from(file_name),
            QString::from(destination_path),
            success,
            result.bytes_transferred,
            QString::from(error_message),
        );
    });
}

fn emit_import_item_finished(
    qt_thread: &QtThread<IOManager>,
    job_id: &str,
    source_path: &str,
    result: TransferItemResult,
    success: bool,
) {
    let job_id = job_id.to_string();
    let file_name = file_name_for_path(source_path);
    let destination_path = result.destination_path;
    let error_message = result.error_message.unwrap_or_default();
    qt_thread.queue(move |mgr| {
        mgr.importItemFinished(
            QString::from(job_id),
            QString::from(file_name),
            QString::from(destination_path),
            success,
            result.bytes_transferred,
            QString::from(error_message),
        );
    });
}

fn finish_export_job(
    qt_thread: &QtThread<IOManager>,
    job_id: String,
    cancelled: bool,
    successful: i32,
    failed: i32,
    total_bytes: i64,
) {
    qt_thread.queue(move |mgr| {
        mgr.exportJobFinished(
            QString::from(job_id),
            cancelled,
            successful,
            failed,
            total_bytes,
        );
    });
}

fn finish_import_job(
    qt_thread: &QtThread<IOManager>,
    job_id: String,
    cancelled: bool,
    successful: i32,
    failed: i32,
    total_bytes: i64,
) {
    qt_thread.queue(move |mgr| {
        mgr.importJobFinished(
            QString::from(job_id),
            cancelled,
            successful,
            failed,
            total_bytes,
        );
    });
}

fn unregister_job(jobs: &Arc<Mutex<HashMap<String, Arc<AtomicBool>>>>, job_id: &str) {
    let mut guard = jobs.lock().expect("IOManager jobs map mutex poisoned");
    if guard.remove(job_id).is_some() {
        debug!("IOManager unregistered job: job_id={job_id}");
    } else {
        warn!("IOManager tried to unregister unknown job: job_id={job_id}");
    }
}
