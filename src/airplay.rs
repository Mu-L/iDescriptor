// SPDX-FileCopyrightText: 2025-2026 Uncore <https://github.com/uncor3>
// SPDX-License-Identifier: AGPL-3.0-or-later

use crate::{
    RUNTIME,
    qt_threading::{QtThread, QtThreading},
};
use log::{debug, error, info};
use macros::QtThreading;
use qmetaobject::prelude::*;
use std::sync::{
    Arc, Mutex,
    atomic::{AtomicU64, Ordering},
};
use tokio::sync::mpsc;
use tokio_util::sync::CancellationToken;

static AIRPLAY_QT_THREAD: once_cell::sync::OnceCell<QtThread<Airplay>> =
    once_cell::sync::OnceCell::new();

#[allow(non_snake_case)]
#[derive(QObject, Default, QtThreading)]
pub struct Airplay {
    base: qt_base_class!(trait QObject),
    init: qt_method!(fn(&self, video_item: QVariant) -> bool),
    cleanup: qt_method!(fn(&self)),
    load_gst_gl: qt_method!(fn(&self) -> bool),
    set_master_volume: qt_method!(fn(&self, volume: f64)),
    connectionChange: qt_signal!(connected: bool),
    connectionDetailsChanged: qt_signal!(name: QString, model: QString, parsed_model: QString, device_id: QString),
    serverReady: qt_signal!(port: i32),
    backendFailed: qt_signal!(code: i32, detail: QString),
    cancellation: Mutex<Option<CancellationToken>>,
    playback: Mutex<Option<Arc<rsplay::GstreamerPlayback>>>,
    generation: Arc<AtomicU64>,
}

impl Airplay {
    fn load_gst_gl(&self) -> bool {
        rsplay::GstreamerPlayback::qml_sink_available()
    }

    fn init(&self, video_item: QVariant) -> bool {
        AIRPLAY_QT_THREAD.get_or_init(|| self.qt_thread());
        self.cleanup();

        let video_item = crate::utils::qvariant_to_ptr(video_item);
        let (events, event_receiver) = mpsc::unbounded_channel();
        //clone avoible?
        let playback = match rsplay::GstreamerPlayback::new(video_item, events.clone()) {
            Ok(playback) => playback,
            Err(err) => {
                error!("Failed to initialize rsplay playback: {err:#}");
                self.backendFailed(-1, QString::from(err.to_string()));
                return false;
            }
        };
        let pairing_store = match rsplay::PersistentPairingStore::open_default() {
            Ok(store) => Arc::new(store),
            Err(err) => {
                error!("Failed to open the rsplay pairing store: {err:#}");
                self.backendFailed(-1, QString::from(err.to_string()));
                return false;
            }
        };
        let receiver = match rsplay::Receiver::new(
            rsplay::ReceiverConfig {
                name: "iDescriptor".to_owned(),
                device_id: pairing_store.device_id(),
                port: 0,
                max_clients: 1,
            },
            playback.clone(),
            pairing_store,
            events,
        ) {
            Ok(receiver) => receiver,
            Err(err) => {
                error!("Failed to configure rsplay: {err:#}");
                self.backendFailed(-1, QString::from(err.to_string()));
                return false;
            }
        };

        let cancellation = CancellationToken::new();
        let generation = self.generation.fetch_add(1, Ordering::AcqRel) + 1;
        *self
            .cancellation
            .lock()
            .expect("AirPlay cancellation lock poisoned") = Some(cancellation.clone());
        *self
            .playback
            .lock()
            .expect("AirPlay playback lock poisoned") = Some(playback);

        let q_thread = AIRPLAY_QT_THREAD
            .get()
            .expect("AirPlay Qt thread must be initialized")
            .clone();
        let event_generation = self.generation.clone();
        RUNTIME.spawn(forward_events(
            event_receiver,
            q_thread.clone(),
            event_generation,
            generation,
        ));

        let run_generation = self.generation.clone();
        RUNTIME.spawn(async move {
            info!("Starting rsplay AirPlay receiver");
            if let Err(err) = receiver.run(cancellation).await {
                error!("rsplay receiver stopped with an error: {err:#}");
                if run_generation.load(Ordering::Acquire) == generation {
                    q_thread.queue(move |t| {
                        if run_generation.load(Ordering::Acquire) == generation {
                            t.backendFailed(-1, QString::from(err.to_string()));
                        }
                    });
                }
            }
        });
        true
    }

    fn cleanup(&self) {
        self.generation.fetch_add(1, Ordering::AcqRel);
        if let Some(cancellation) = self
            .cancellation
            .lock()
            .expect("AirPlay cancellation lock poisoned")
            .take()
        {
            debug!("Stopping rsplay AirPlay receiver");
            cancellation.cancel();
        }
        self.playback
            .lock()
            .expect("AirPlay playback lock poisoned")
            .take();
        self.connectionChange(false);
    }

    fn set_master_volume(&self, volume: f64) {
        let volume = volume.clamp(0.0, 1.0) as f32;
        debug!("Setting rsplay master volume to {:.0}%", volume * 100.0);
        if let Some(playback) = self
            .playback
            .lock()
            .expect("AirPlay playback lock poisoned")
            .as_ref()
        {
            playback.set_volume(volume);
        }
    }
}

async fn forward_events(
    mut events: mpsc::UnboundedReceiver<rsplay::ReceiverEvent>,
    q_thread: QtThread<Airplay>,
    active_generation: Arc<AtomicU64>,
    generation: u64,
) {
    while let Some(event) = events.recv().await {
        if active_generation.load(Ordering::Acquire) != generation {
            return;
        }

        match event {
            rsplay::ReceiverEvent::Ready { port } => q_thread.queue({
                let active_generation = active_generation.clone();
                move |t| {
                    if active_generation.load(Ordering::Acquire) == generation {
                        t.serverReady(port as i32);
                    }
                }
            }),
            rsplay::ReceiverEvent::ClientConnected { address } => {
                debug!("rsplay AirPlay client connected from {address}");
                q_thread.queue({
                    let active_generation = active_generation.clone();
                    move |t| {
                        if active_generation.load(Ordering::Acquire) == generation {
                            t.connectionChange(true);
                        }
                    }
                });
            }
            rsplay::ReceiverEvent::ClientDetails {
                device_id,
                model,
                name,
            } => {
                let parsed_model = crate::device_db::find_by_identifier(&model);
                q_thread.queue({
                    let active_generation = active_generation.clone();
                    move |t| {
                        if active_generation.load(Ordering::Acquire) == generation {
                            t.connectionDetailsChanged(
                                QString::from(name),
                                QString::from(model),
                                QString::from(
                                    parsed_model
                                        .unwrap_or(&crate::device_db::UNKNOWN_DEVICE)
                                        .display_name,
                                ),
                                QString::from(device_id),
                            );
                        }
                    }
                });
            }
            rsplay::ReceiverEvent::ClientDisconnected => q_thread.queue({
                let active_generation = active_generation.clone();
                move |t| {
                    if active_generation.load(Ordering::Acquire) == generation {
                        t.connectionChange(false);
                    }
                }
            }),
            rsplay::ReceiverEvent::Error(detail) => q_thread.queue({
                let active_generation = active_generation.clone();
                move |t| {
                    if active_generation.load(Ordering::Acquire) == generation {
                        t.backendFailed(-1, QString::from(detail));
                    }
                }
            }),
            rsplay::ReceiverEvent::Stopped => debug!("rsplay AirPlay receiver stopped"),
        }
    }
}
