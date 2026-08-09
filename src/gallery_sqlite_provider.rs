// SPDX-FileCopyrightText: 2025-2026 Uncore <https://github.com/uncor3>
// SPDX-License-Identifier: AGPL-3.0-or-later

use crate::constants::{
    ALBUM_CONTENTS_QUERY_TEMPLATE, FAVS_ALBUM_ID, FAVS_ALBUM_QUERY, FAVS_QUERY,
    GALLERY_TOTAL_SIZE_QUERY, HIDDEN_ALBUM_ID, HIDDEN_ALBUM_QUERY, HIDDEN_QUERY,
    IOS_15_ALBUM_QUERY_STATEMENT, IOS_26_ALBUM_QUERY_STATEMENT, PHOTOS_SQLITE_REMOTE_PATH,
    PHOTOS_SQLITE_SHM_REMOTE_PATH, PHOTOS_SQLITE_WAL_REMOTE_PATH, RECENTLY_DELETED_ALBUM_ID,
    RECENTLY_DELETED_ALBUM_QUERY, RECENTLY_DELETED_QUERY, RECENTS_ALBUM_ID, RECENTS_ALBUM_QUERY,
    RECENTS_QUERY, SQLITE_GALLERY_PROVIDER_NAME, SQLITE_VFS_GALLERY_PROVIDER_NAME,
};
use crate::gallery::{
    GalleryAlbum, GalleryFuture, GalleryMediaFilter, GalleryProvider, export_afc_file,
    matches_media_filter,
};
use crate::gallery_sqlite_vfs::{GalleryVfsRegistration, open_gallery_vfs_connection};
use crate::utils::TempDirGuard;
use ::log::{debug, info, warn};
use anyhow::{Context, anyhow};
use idevice::IdeviceError;
use idevice::afc::AfcClient;
use idevice::afc::errors::AfcError;
use idevice::provider::IdeviceProvider;
use rusqlite::{Connection, OptionalExtension};
use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::sync::Arc;
use tokio::sync::Mutex;

#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
enum SnapshotFile {
    Database,
    Wal,
    Shm,
}

impl SnapshotFile {
    const ALL: [Self; 3] = [Self::Database, Self::Wal, Self::Shm];

    fn remote_path(self) -> &'static str {
        match self {
            Self::Database => PHOTOS_SQLITE_REMOTE_PATH,
            Self::Wal => PHOTOS_SQLITE_WAL_REMOTE_PATH,
            Self::Shm => PHOTOS_SQLITE_SHM_REMOTE_PATH,
        }
    }

    fn local_name(self) -> &'static str {
        match self {
            Self::Database => "Photos.sqlite",
            Self::Wal => "Photos.sqlite-wal",
            Self::Shm => "Photos.sqlite-shm",
        }
    }

    fn is_required(self) -> bool {
        self == Self::Database
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
struct RemoteFileMetadata {
    size: usize,
    modified: String,
}

type SnapshotMetadata = HashMap<SnapshotFile, Option<RemoteFileMetadata>>;

struct SqliteProviderState {
    connection: Option<Connection>,
    vfs_registration: Option<GalleryVfsRegistration>,
    assets_table_name: String,
    assets_table_album_column: String,
    committed_metadata: SnapshotMetadata,
}

#[derive(Clone)]
enum SqliteProviderSource {
    Snapshot {
        temp_dir: PathBuf,
    },
    Vfs {
        provider: Arc<Mutex<Box<dyn IdeviceProvider>>>,
    },
}

pub struct SqliteGalleryProvider {
    state: Arc<Mutex<SqliteProviderState>>,
    refresh_lock: Arc<Mutex<()>>,
    afc: Arc<Mutex<AfcClient>>,
    source: SqliteProviderSource,
    ios_version: u32,
    name: String,
}

impl GalleryProvider for SqliteGalleryProvider {
    fn name(&self) -> String {
        self.name.clone()
    }

    fn read_albums(&self) -> GalleryFuture<(Vec<GalleryAlbum>, i32)> {
        let state = self.state.clone();
        let ios_ver = self.ios_version;

        Box::pin(async move {
            debug!("Reading via Sqlite provider for ios {ios_ver}");
            let state = state.lock().await;
            let conn = state
                .connection
                .as_ref()
                .context("SQLite gallery connection is closed")?;
            tokio::task::block_in_place(|| {
                read_albums_from_connection(
                    conn,
                    ios_ver,
                    &state.assets_table_name,
                    &state.assets_table_album_column,
                )
            })
        })
    }

    fn reload(&self) -> GalleryFuture<(Vec<GalleryAlbum>, i32)> {
        let state = self.state.clone();
        let refresh_lock = self.refresh_lock.clone();
        let afc = self.afc.clone();
        let source = self.source.clone();
        let ios_version = self.ios_version;

        Box::pin(async move {
            let _refresh_guard = refresh_lock.lock().await;
            if let SqliteProviderSource::Vfs { provider } = source {
                let mut state = state.lock().await;
                tokio::task::block_in_place(|| close_connection(&mut state))?;
                state.vfs_registration = None;

                let (connection, registration) = open_gallery_vfs_connection(afc, provider).await?;
                let prepared = tokio::task::block_in_place(|| {
                    (|| -> anyhow::Result<_> {
                        let (assets_table_name, assets_table_album_column) =
                            validate_vfs_connection(&connection)?;
                        let albums = read_albums_from_connection(
                            &connection,
                            ios_version,
                            &assets_table_name,
                            &assets_table_album_column,
                        )?;
                        Ok((assets_table_name, assets_table_album_column, albums))
                    })()
                });
                let (assets_table_name, assets_table_album_column, albums) = match prepared {
                    Ok(prepared) => prepared,
                    Err(error) => {
                        drop(connection);
                        drop(registration);
                        return Err(error);
                    }
                };

                state.connection = Some(connection);
                state.vfs_registration = Some(registration);
                state.assets_table_name = assets_table_name;
                state.assets_table_album_column = assets_table_album_column;
                return Ok(albums);
            }

            let SqliteProviderSource::Snapshot { temp_dir } = source else {
                unreachable!();
            };
            let remote_metadata = {
                let mut afc = afc.lock().await;
                read_remote_snapshot_metadata(&mut afc).await?
            };

            let mut state = state.lock().await;
            close_connection(&mut state)?;

            let mut afc = afc.lock().await;
            for snapshot_file in SnapshotFile::ALL {
                let local_path = temp_dir.join(snapshot_file.local_name());
                let remote_file_metadata = remote_metadata.get(&snapshot_file).cloned().flatten();

                match remote_file_metadata {
                    Some(_) => {
                        if snapshot_file_changed(
                            &state.committed_metadata,
                            &remote_metadata,
                            snapshot_file,
                        ) {
                            info!(
                                "Reloading changed gallery snapshot file {}",
                                snapshot_file.remote_path()
                            );
                            export_afc_file(&mut afc, snapshot_file.remote_path(), &local_path)
                                .await
                                .with_context(|| {
                                    format!("Failed to refresh {}", snapshot_file.remote_path())
                                })?;
                        }
                    }
                    None => {
                        if snapshot_file.is_required() {
                            anyhow::bail!("Required Photos.sqlite is missing from the device");
                        }
                        if local_path.exists() {
                            tokio::fs::remove_file(&local_path).await.with_context(|| {
                                format!("Failed to remove stale {}", local_path.display())
                            })?;
                        }
                    }
                }
            }
            drop(afc);

            let database_path = temp_dir.join(SnapshotFile::Database.local_name());
            let (connection, assets_table_name, assets_table_album_column) =
                open_and_validate_connection(&database_path)?;
            let albums = read_albums_from_connection(
                &connection,
                ios_version,
                &assets_table_name,
                &assets_table_album_column,
            )?;

            state.connection = Some(connection);
            state.vfs_registration = None;
            state.assets_table_name = assets_table_name;
            state.assets_table_album_column = assets_table_album_column;
            state.committed_metadata = remote_metadata;
            Ok(albums)
        })
    }

    fn query_album(
        &self,
        id: i32,
        media_filter: GalleryMediaFilter,
        most_recent_first: bool,
    ) -> GalleryFuture<Vec<String>> {
        let state = self.state.clone();

        Box::pin(async move {
            match id {
                FAVS_ALBUM_ID => query_favs_album(state, media_filter, most_recent_first).await,
                RECENTS_ALBUM_ID => {
                    query_recents_album(state, media_filter, most_recent_first).await
                }
                RECENTLY_DELETED_ALBUM_ID => {
                    query_recently_deleted_album(state, media_filter, most_recent_first).await
                }
                HIDDEN_ALBUM_ID => query_hidden_album(state, media_filter, most_recent_first).await,
                _ => query_sqlite_album(state, id, media_filter, most_recent_first).await,
            }
        })
    }

    fn query_gallery_size(&self) -> GalleryFuture<u64> {
        let state = self.state.clone();
        Box::pin(async move {
            let state = state.lock().await;
            let conn = state
                .connection
                .as_ref()
                .context("SQLite gallery connection is closed")?;
            let total_size: i64 = tokio::task::block_in_place(|| {
                conn.query_row(GALLERY_TOTAL_SIZE_QUERY, [], |r| r.get(0))
            })?;
            u64::try_from(total_size).context("Gallery size cannot be negative")
        })
    }
}

impl Drop for SqliteGalleryProvider {
    fn drop(&mut self) {
        if let Ok(mut state) = self.state.try_lock() {
            let _ = close_connection(&mut state);
            state.vfs_registration = None;
        }

        if let SqliteProviderSource::Snapshot { temp_dir } = &self.source {
            if let Err(e) = std::fs::remove_dir_all(temp_dir) {
                println!("Failed to remove temp gallery database dir: {}", e);
            }
        }
    }
}

pub async fn build_sqlite_provider(
    afc: Arc<Mutex<AfcClient>>,
    ios_version: u32,
) -> anyhow::Result<Arc<dyn GalleryProvider>> {
    let temp_dir =
        std::env::temp_dir().join(format!("idescriptor-photos-db-{}", uuid::Uuid::new_v4()));
    tokio::fs::create_dir_all(&temp_dir).await?;
    // this is so that if we fail to export the db, we don't leave a temp dir behind
    let temp_dir_guard = TempDirGuard::new(temp_dir);

    let remote_metadata = {
        let mut afc_guard = afc.lock().await;
        let metadata = read_remote_snapshot_metadata(&mut afc_guard).await?;
        for snapshot_file in SnapshotFile::ALL {
            match metadata.get(&snapshot_file).cloned().flatten() {
                Some(_) => {
                    export_afc_file(
                        &mut afc_guard,
                        snapshot_file.remote_path(),
                        &temp_dir_guard.path().join(snapshot_file.local_name()),
                    )
                    .await
                    .with_context(|| format!("Failed to export {}", snapshot_file.remote_path()))?;
                }
                None if snapshot_file.is_required() => {
                    anyhow::bail!("Required Photos.sqlite is missing from the device");
                }
                None => {}
            }
        }
        metadata
    };

    let gallery_db_path = temp_dir_guard
        .path()
        .join(SnapshotFile::Database.local_name());
    let (connection, assets_table_name, assets_table_album_column) =
        open_and_validate_connection(&gallery_db_path)?;

    let temp_dir = temp_dir_guard.keep();
    Ok(Arc::new(SqliteGalleryProvider {
        state: Arc::new(Mutex::new(SqliteProviderState {
            connection: Some(connection),
            vfs_registration: None,
            assets_table_name,
            assets_table_album_column,
            committed_metadata: remote_metadata,
        })),
        refresh_lock: Arc::new(Mutex::new(())),
        afc,
        source: SqliteProviderSource::Snapshot { temp_dir },
        ios_version,
        name: SQLITE_GALLERY_PROVIDER_NAME.into(),
    }))
}

pub async fn build_sqlite_vfs_provider(
    afc: Arc<Mutex<AfcClient>>,
    provider: Arc<Mutex<Box<dyn IdeviceProvider>>>,
    ios_version: u32,
) -> anyhow::Result<Arc<dyn GalleryProvider>> {
    let (connection, registration) =
        open_gallery_vfs_connection(afc.clone(), provider.clone()).await?;
    let (assets_table_name, assets_table_album_column) =
        match tokio::task::block_in_place(|| validate_vfs_connection(&connection)) {
            Ok(schema) => schema,
            Err(error) => {
                drop(connection);
                drop(registration);
                return Err(error);
            }
        };

    Ok(Arc::new(SqliteGalleryProvider {
        state: Arc::new(Mutex::new(SqliteProviderState {
            connection: Some(connection),
            vfs_registration: Some(registration),
            assets_table_name,
            assets_table_album_column,
            committed_metadata: SnapshotMetadata::new(),
        })),
        refresh_lock: Arc::new(Mutex::new(())),
        afc,
        source: SqliteProviderSource::Vfs { provider },
        ios_version,
        name: SQLITE_VFS_GALLERY_PROVIDER_NAME.into(),
    }))
}

async fn read_remote_snapshot_metadata(afc: &mut AfcClient) -> anyhow::Result<SnapshotMetadata> {
    let mut metadata = SnapshotMetadata::new();
    for snapshot_file in SnapshotFile::ALL {
        match afc.get_file_info(snapshot_file.remote_path()).await {
            Ok(info) => {
                metadata.insert(
                    snapshot_file,
                    Some(RemoteFileMetadata {
                        size: info.size,
                        modified: info.modified.to_string(),
                    }),
                );
            }
            Err(err) if snapshot_file.is_required() || !is_missing_file_error(&err) => {
                return Err(err)
                    .with_context(|| format!("Failed to stat {}", snapshot_file.remote_path()));
            }
            Err(err) => {
                warn!(
                    "Optional gallery snapshot file {} is unavailable: {}",
                    snapshot_file.remote_path(),
                    err
                );
                metadata.insert(snapshot_file, None);
            }
        }
    }
    Ok(metadata)
}

fn is_missing_file_error(error: &IdeviceError) -> bool {
    matches!(
        error,
        IdeviceError::NotFound | IdeviceError::Afc(AfcError::ObjectNotFound)
    )
}

fn snapshot_file_changed(
    committed: &SnapshotMetadata,
    current: &SnapshotMetadata,
    snapshot_file: SnapshotFile,
) -> bool {
    committed.get(&snapshot_file).cloned().flatten()
        != current.get(&snapshot_file).cloned().flatten()
}

fn close_connection(state: &mut SqliteProviderState) -> anyhow::Result<()> {
    let Some(connection) = state.connection.take() else {
        return Ok(());
    };

    match connection.close() {
        Ok(()) => Ok(()),
        Err((connection, err)) => {
            state.connection = Some(connection);
            Err(anyhow!("Failed to close SQLite gallery connection: {err}"))
        }
    }
}

fn open_and_validate_connection(path: &Path) -> anyhow::Result<(Connection, String, String)> {
    let connection = Connection::open(path)
        .with_context(|| format!("Failed to open gallery database {}", path.display()))?;
    let quick_check: String = connection
        .query_row("PRAGMA quick_check", [], |row| row.get(0))
        .context("Failed to validate refreshed gallery database")?;
    if quick_check != "ok" {
        anyhow::bail!("Gallery database validation failed: {quick_check}");
    }

    let (assets_table_name, assets_table_album_column) = discover_assets_table(&connection)?;
    Ok((connection, assets_table_name, assets_table_album_column))
}

fn validate_vfs_connection(connection: &Connection) -> anyhow::Result<(String, String)> {
    // TODO: do we need this?
    // connection
    //     .query_row("SELECT COUNT(*) FROM sqlite_master", [], |row| {
    //         row.get::<_, i64>(0)
    //     })
    //     .context("Failed to read the gallery schema through the AFC VFS")?;
    discover_assets_table(connection)
}

fn discover_assets_table(connection: &Connection) -> anyhow::Result<(String, String)> {
    /*
        We need to get the dynamic asset table name from the database.
        iOS seems to be bumping the version with every major iOS update.
    */
    let mut statement = connection.prepare("SELECT name FROM sqlite_master WHERE type='table'")?;
    let tables = statement.query_map([], |row| row.get::<_, String>(0))?;
    let table_pattern = regex::Regex::new(r"^Z_\d+ASSETS$")?;

    for table in tables {
        let table_name = table?;
        if !table_pattern.is_match(&table_name) {
            continue;
        }

        let prefix = table_name
            .strip_suffix("ASSETS")
            .ok_or_else(|| anyhow!("Couldn't derive the album column from {table_name}"))?;
        let album_column = format!("{prefix}ALBUMS");
        info!(
            "Found gallery relationship table {} with album column {}",
            table_name, album_column
        );
        return Ok((table_name, album_column));
    }

    Err(anyhow!(
        "Couldn't find the gallery assets relationship table"
    ))
}

fn read_albums_from_connection(
    connection: &Connection,
    ios_version: u32,
    assets_table_name: &str,
    assets_table_album_column: &str,
) -> anyhow::Result<(Vec<GalleryAlbum>, i32)> {
    let mut albums = Vec::new();
    let mut failed_albums_count = 0;

    let (fname, fdir, count) = explore_recents_album(connection)?;
    albums.push(GalleryAlbum {
        id: RECENTS_ALBUM_ID,
        name: String::from("Recents"),
        item_count: count,
        preview_path: join_device_path(&fdir, &fname),
    });

    let (other_albums, other_failed_albums_count) = explore_other_albums(
        connection,
        ios_version,
        assets_table_name,
        assets_table_album_column,
    )?;
    albums.extend(other_albums);
    failed_albums_count += other_failed_albums_count;

    let (fname, fdir, count) = explore_favs_album(connection)?;
    albums.push(GalleryAlbum {
        id: FAVS_ALBUM_ID,
        name: String::from("Favorites"),
        item_count: count,
        preview_path: join_device_path(&fdir, &fname),
    });

    let (fname, fdir, count) = explore_hidden_album(connection)?;
    albums.push(GalleryAlbum {
        id: HIDDEN_ALBUM_ID,
        name: String::from("Hidden"),
        item_count: count,
        preview_path: join_device_path(&fdir, &fname),
    });

    let (fname, fdir, count) = explore_recently_deleted(connection)?;
    albums.push(GalleryAlbum {
        id: RECENTLY_DELETED_ALBUM_ID,
        name: String::from("Recently Deleted"),
        item_count: count,
        preview_path: join_device_path(&fdir, &fname),
    });

    debug!("Failed to read {failed_albums_count} albums");
    Ok((albums, failed_albums_count))
}

async fn query_sqlite_album(
    state: Arc<Mutex<SqliteProviderState>>,
    id: i32,
    media_filter: GalleryMediaFilter,
    most_recent_first: bool,
) -> anyhow::Result<Vec<String>> {
    let state = state.lock().await;
    tokio::task::block_in_place(|| {
        let connection = state
            .connection
            .as_ref()
            .context("SQLite gallery connection is closed")?;
        let mut paths = Vec::new();
        let query = format!(
            "{} ORDER BY ZASSET.Z_PK {}",
            ALBUM_CONTENTS_QUERY_TEMPLATE
                .replace("{table}", &state.assets_table_name)
                .replace("{album}", &state.assets_table_album_column),
            sqlite_order_direction(most_recent_first),
        );
        debug!("Executing query: {}", query);
        debug!("With album id: {}", id);
        let mut stmt = connection.prepare(&query)?;

        let row_iter = stmt.query_map([id], |r| {
            let fdir: String = r.get(0)?;
            let fname: String = r.get(1)?;
            Ok((fdir, fname))
        })?;

        for item in row_iter {
            let (fdir, fname) = item?;
            let path = join_device_path(&fdir, &fname);
            if matches_media_filter(&path, media_filter) {
                paths.push(path);
            }
        }

        Ok(paths)
    })
}

fn sqlite_order_direction(most_recent_first: bool) -> &'static str {
    if most_recent_first { "DESC" } else { "ASC" }
}

fn sqlite_ordered_query(query: &str, most_recent_first: bool) -> String {
    query.replace(
        "ORDER BY ZASSET.Z_PK DESC",
        &format!(
            "ORDER BY ZASSET.Z_PK {}",
            sqlite_order_direction(most_recent_first)
        ),
    )
}

async fn query_favs_album(
    state: Arc<Mutex<SqliteProviderState>>,
    media_filter: GalleryMediaFilter,
    most_recent_first: bool,
) -> anyhow::Result<Vec<String>> {
    let state = state.lock().await;
    tokio::task::block_in_place(|| {
        let connection = state
            .connection
            .as_ref()
            .context("SQLite gallery connection is closed")?;
        let mut paths = Vec::new();

        //favs album
        let query = sqlite_ordered_query(FAVS_QUERY, most_recent_first);
        let mut favs_stmt = connection.prepare(&query)?;

        let favs_iter = favs_stmt.query_map([], |r| {
            let fname: String = r.get(0)?;
            let fdir: String = r.get(1)?;
            Ok((fname, fdir))
        })?;

        for fav_item in favs_iter {
            let (fname, fdir) = fav_item?;
            let path = join_device_path(&fdir, &fname);
            if matches_media_filter(&path, media_filter) {
                paths.push(path);
            }
        }

        Ok(paths)
    })
}

async fn query_recents_album(
    state: Arc<Mutex<SqliteProviderState>>,
    media_filter: GalleryMediaFilter,
    most_recent_first: bool,
) -> anyhow::Result<Vec<String>> {
    let state = state.lock().await;
    tokio::task::block_in_place(|| {
        let connection = state
            .connection
            .as_ref()
            .context("SQLite gallery connection is closed")?;
        let mut paths = Vec::new();

        //recents album
        let query = sqlite_ordered_query(RECENTS_QUERY, most_recent_first);
        let mut recents_stmt = connection.prepare(&query)?;

        let recents_iter = recents_stmt.query_map([], |r| {
            let fname: String = r.get(0)?;
            let fdir: String = r.get(1)?;
            Ok((fname, fdir))
        })?;

        for recent_item in recents_iter {
            let (fname, fdir) = recent_item?;
            let path = join_device_path(&fdir, &fname);
            if matches_media_filter(&path, media_filter) {
                paths.push(path);
            }
        }

        Ok(paths)
    })
}

async fn query_hidden_album(
    state: Arc<Mutex<SqliteProviderState>>,
    media_filter: GalleryMediaFilter,
    most_recent_first: bool,
) -> anyhow::Result<Vec<String>> {
    let state = state.lock().await;
    tokio::task::block_in_place(|| {
        let connection = state
            .connection
            .as_ref()
            .context("SQLite gallery connection is closed")?;
        let mut paths = Vec::new();

        let query = sqlite_ordered_query(HIDDEN_QUERY, most_recent_first);
        let mut hidden_stmt = connection.prepare(&query)?;

        let hidden_iter = hidden_stmt.query_map([], |r| {
            let fname: String = r.get(0)?;
            let fdir: String = r.get(1)?;
            Ok((fname, fdir))
        })?;

        for hidden_item in hidden_iter {
            let (fname, fdir) = hidden_item?;
            let path = join_device_path(&fdir, &fname);
            if matches_media_filter(&path, media_filter) {
                paths.push(path);
            }
        }

        Ok(paths)
    })
}

async fn query_recently_deleted_album(
    state: Arc<Mutex<SqliteProviderState>>,
    media_filter: GalleryMediaFilter,
    most_recent_first: bool,
) -> anyhow::Result<Vec<String>> {
    let state = state.lock().await;
    tokio::task::block_in_place(|| {
        let connection = state
            .connection
            .as_ref()
            .context("SQLite gallery connection is closed")?;
        let mut paths = Vec::new();

        //recently deleted album
        let query = sqlite_ordered_query(RECENTLY_DELETED_QUERY, most_recent_first);
        let mut recently_deleted_stmt = connection.prepare(&query)?;

        let recently_deleted_iter = recently_deleted_stmt.query_map([], |r| {
            let fname: String = r.get(0)?;
            let fdir: String = r.get(1)?;
            Ok((fname, fdir))
        })?;

        for deleted_item in recently_deleted_iter {
            let (fname, fdir) = deleted_item?;
            let path = join_device_path(&fdir, &fname);
            if matches_media_filter(&path, media_filter) {
                paths.push(path);
            }
        }

        Ok(paths)
    })
}

fn join_device_path(dir: &str, file_name: &str) -> String {
    format!("{}/{}", dir.trim_end_matches('/'), file_name)
}

//used in init
fn explore_recents_album(conn: &Connection) -> anyhow::Result<(String, String, i32)> {
    let mut recents_stmt = conn.prepare(RECENTS_ALBUM_QUERY)?;

    let recents_row = recents_stmt
        .query_row([], |r| {
            let fname: String = r.get(0)?;
            let fdir: String = r.get(1)?;
            let count: i32 = r.get(2)?;
            Ok((fname, fdir, count))
        })
        .optional()?
        .unwrap_or_default();

    Ok(recents_row)
}

fn explore_favs_album(conn: &Connection) -> anyhow::Result<(String, String, i32)> {
    let mut favs_stmt = conn.prepare(FAVS_ALBUM_QUERY)?;

    let favs_row = favs_stmt
        .query_row([], |r| {
            let fname: String = r.get(0)?;
            let fdir: String = r.get(1)?;
            let count: i32 = r.get(2)?;
            Ok((fname, fdir, count))
        })
        .optional()?
        .unwrap_or_default();

    Ok(favs_row)
}

fn explore_hidden_album(conn: &Connection) -> anyhow::Result<(String, String, i32)> {
    let mut hidden_stmt = conn.prepare(HIDDEN_ALBUM_QUERY)?;

    let hidden_row = hidden_stmt
        .query_row([], |r| {
            let fname: String = r.get(0)?;
            let fdir: String = r.get(1)?;
            let count: i32 = r.get(2)?;
            Ok((fname, fdir, count))
        })
        .optional()?
        .unwrap_or_default();

    Ok(hidden_row)
}

fn explore_recently_deleted(conn: &Connection) -> anyhow::Result<(String, String, i32)> {
    let mut recently_deleted_stmt = conn.prepare(RECENTLY_DELETED_ALBUM_QUERY)?;

    let recently_deleted_row = recently_deleted_stmt
        .query_row([], |r| {
            let fname: String = r.get(0)?;
            let fdir: String = r.get(1)?;
            let count: i32 = r.get(2)?;
            Ok((fname, fdir, count))
        })
        .optional()?
        .unwrap_or_default();

    Ok(recently_deleted_row)
}

fn explore_other_albums(
    conn: &Connection,
    ios_ver: u32,
    assets_table_name: &str,
    assets_table_album_column: &str,
) -> anyhow::Result<(Vec<GalleryAlbum>, i32)> {
    let query = album_query(ios_ver, assets_table_name, assets_table_album_column);

    let mut stmt = conn.prepare(&query)?;
    let rows_iter = stmt.query_map([], |row| {
        let album_id: i32 = row.get(0)?;
        let title: String = row.get(1)?;
        let item_count: i32 = row.get(2)?;
        let asset_dir: String = row.get(3)?;
        let asset_file_name: String = row.get(4)?;
        Ok((album_id, title, item_count, asset_dir, asset_file_name))
    })?;

    let mut albums = Vec::new();
    let mut failed_albums_count = 0;
    for row_res in rows_iter {
        match row_res {
            Ok((album_id, title, item_count, asset_dir, asset_file_name)) => {
                println!(
                    "Found album: {title} with {item_count} items, preview: {asset_dir}/{asset_file_name}"
                );
                albums.push(GalleryAlbum {
                    id: album_id,
                    name: title,
                    item_count,
                    preview_path: join_device_path(&asset_dir, &asset_file_name),
                });
            }
            Err(_) => failed_albums_count += 1,
        }
    }

    Ok((albums, failed_albums_count))
}

fn album_query(
    ios_version: u32,
    assets_table_name: &str,
    assets_table_album_column: &str,
) -> String {
    if ios_version <= 15 {
        return IOS_15_ALBUM_QUERY_STATEMENT.to_string();
    }

    IOS_26_ALBUM_QUERY_STATEMENT
        .replace("{table}", assets_table_name)
        .replace("{album}", assets_table_album_column)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn metadata(size: usize, modified: &str) -> RemoteFileMetadata {
        RemoteFileMetadata {
            size,
            modified: modified.to_string(),
        }
    }

    #[test]
    fn snapshot_change_detection_uses_presence_size_and_modified_time() {
        let mut committed = SnapshotMetadata::new();
        committed.insert(SnapshotFile::Database, Some(metadata(100, "1")));
        committed.insert(SnapshotFile::Wal, None);
        committed.insert(SnapshotFile::Shm, Some(metadata(50, "2")));

        let unchanged = committed.clone();
        assert!(!snapshot_file_changed(
            &committed,
            &unchanged,
            SnapshotFile::Database
        ));

        let mut size_changed = committed.clone();
        size_changed.insert(SnapshotFile::Database, Some(metadata(101, "1")));
        assert!(snapshot_file_changed(
            &committed,
            &size_changed,
            SnapshotFile::Database
        ));

        let mut modified_changed = committed.clone();
        modified_changed.insert(SnapshotFile::Shm, Some(metadata(50, "3")));
        assert!(snapshot_file_changed(
            &committed,
            &modified_changed,
            SnapshotFile::Shm
        ));

        let mut appeared = committed.clone();
        appeared.insert(SnapshotFile::Wal, Some(metadata(10, "4")));
        assert!(snapshot_file_changed(
            &committed,
            &appeared,
            SnapshotFile::Wal
        ));

        let mut disappeared = committed.clone();
        disappeared.insert(SnapshotFile::Shm, None);
        assert!(snapshot_file_changed(
            &committed,
            &disappeared,
            SnapshotFile::Shm
        ));
    }

    #[test]
    fn invalid_database_fails_quick_check() {
        let path = std::env::temp_dir().join(format!(
            "idescriptor-invalid-gallery-{}.sqlite",
            uuid::Uuid::new_v4()
        ));
        std::fs::write(&path, b"not a sqlite database").unwrap();

        assert!(open_and_validate_connection(&path).is_err());
        std::fs::remove_file(path).unwrap();
    }

    #[test]
    fn incomplete_database_fails_album_query_validation() {
        let path = std::env::temp_dir().join(format!(
            "idescriptor-incomplete-gallery-{}.sqlite",
            uuid::Uuid::new_v4()
        ));
        let connection = Connection::open(&path).unwrap();
        connection
            .execute("CREATE TABLE Z_1ASSETS (Z_PK INTEGER)", [])
            .unwrap();
        connection.close().unwrap();

        let (connection, assets_table_name, assets_table_album_column) =
            open_and_validate_connection(&path).unwrap();
        assert!(
            read_albums_from_connection(
                &connection,
                26,
                &assets_table_name,
                &assets_table_album_column,
            )
            .is_err()
        );
        connection.close().unwrap();
        std::fs::remove_file(path).unwrap();
    }

    #[test]
    fn modern_album_query_replaces_dynamic_schema_placeholders() {
        let query = album_query(26, "Z_33ASSETS", "Z_33ALBUMS");

        assert!(!query.contains("{table}"));
        assert!(!query.contains("{album}"));
        assert!(query.contains("Z_33ASSETS.Z_3ASSETS"));
        assert!(query.contains("Z_33ASSETS.Z_33ALBUMS"));
    }

    #[test]
    fn built_in_album_content_queries_have_valid_clause_order() {
        let connection = Connection::open_in_memory().unwrap();
        connection
            .execute(
                "CREATE TABLE ZASSET (
                    Z_PK INTEGER,
                    ZFILENAME TEXT,
                    ZDIRECTORY TEXT,
                    ZFAVORITE INTEGER,
                    ZTRASHEDSTATE INTEGER,
                    ZVISIBILITYSTATE INTEGER,
                    ZHIDDEN INTEGER
                )",
                [],
            )
            .unwrap();

        for query in [
            RECENTS_QUERY,
            FAVS_QUERY,
            RECENTLY_DELETED_QUERY,
            HIDDEN_QUERY,
        ] {
            connection
                .prepare(&sqlite_ordered_query(query, true))
                .unwrap();
            connection
                .prepare(&sqlite_ordered_query(query, false))
                .unwrap();
        }

        connection.prepare(HIDDEN_ALBUM_QUERY).unwrap();
    }
}
