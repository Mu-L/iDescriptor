// SPDX-FileCopyrightText: 2025-2026 Uncore <https://github.com/uncor3>
// SPDX-License-Identifier: AGPL-3.0-or-later

use crate::{
    RUNTIME,
    qt_threading::{QtThread, QtThreading},
};
use log::{debug, error};
use macros::QtThreading;
use qmetaobject::prelude::*;
use qttypes::QStringList;

use once_cell::sync::OnceCell;
use std::ffi::{CStr, CString};
use std::os::raw::c_void;
use std::os::raw::{c_char, c_int};
use std::sync::atomic::AtomicUsize;
use std::sync::atomic::Ordering;

static VIDEO_ITEM_PTR: AtomicUsize = AtomicUsize::new(0);
static AIRPLAY_QT_THREAD: OnceCell<QtThread<Airplay>> = OnceCell::new();

unsafe extern "C" {
    fn init_uxplay(argc: c_int, argv: *mut *mut c_char) -> c_int;
    fn uxplay_cleanup();
    fn uxplay_set_audio_volume(volume: f64);

    fn set_uxplay_gl_callbacks(
        connection_cb: extern "C" fn(bool),
        get_video_item_cb: extern "C" fn() -> *mut c_void,
        connection_details_cb: extern "C" fn(*const c_char, *const c_char, *const c_char),
    );
}

extern "C" fn rust_uxplay_get_video_item() -> *mut c_void {
    VIDEO_ITEM_PTR.load(Ordering::Acquire) as *mut c_void
}

extern "C" fn rust_uxplay_connection_cb(connected: bool) {
    debug!("AirPlay connection changed: {}", connected);
    if let Some(q_thread) = AIRPLAY_QT_THREAD.get() {
        q_thread.queue(move |t| {
            t.connection_change(connected);
        });
    }
}

extern "C" fn rust_uxplay_connection_details_cb(
    device_id: *const c_char,
    model: *const c_char,
    name: *const c_char,
) {
    let copy_string = |value: *const c_char| {
        if value.is_null() {
            String::new()
        } else {
            unsafe { CStr::from_ptr(value) }
                .to_string_lossy()
                .into_owned()
        }
    };
    let device_id = copy_string(device_id);
    let model = copy_string(model);
    let parsed_model = crate::device_db::find_by_identifier(&model);
    let name = copy_string(name);

    debug!(
        "AirPlay client details received: name={:?}, model={:?}, device_id={:?}",
        name, model, device_id
    );
    if let Some(q_thread) = AIRPLAY_QT_THREAD.get() {
        q_thread.queue(move |t| {
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
        });
    }
}

#[allow(non_snake_case)]
#[derive(QObject, Default, QtThreading)]
pub struct Airplay {
    base: qt_base_class!(trait QObject),
    init: qt_method!(fn(&self, video_item: QVariant) -> bool),
    cleanup: qt_method!(fn(&self)),
    load_gst_gl: qt_method!(fn(&self) -> bool),
    set_master_volume: qt_method!(fn(&self, volume: f64)),
    launch_arguments: qt_method!(fn(&self) -> QStringList),
    check_requirements: qt_method!(fn(&self)),
    connection_change: qt_signal!(connected: bool),
    connectionDetailsChanged: qt_signal!(name: QString, model: QString, parsed_model: QString, device_id: QString),
    requirementsChecked: qt_signal!(ready: bool, dependency_id: QString, reason: QString, detail: QString),
    backendFailed: qt_signal!(code: i32, detail: QString),
}

impl Airplay {
    fn check_requirements(&self) {
        let q_thread = self.qt_thread();
        RUNTIME.spawn(async move {
            #[cfg(not(target_os = "macos"))]
            let result = crate::diagnose::check_airplay_requirement().await;

            q_thread.queue(move |t| {
                #[cfg(target_os = "macos")]
                t.requirementsChecked(
                    true,
                    QString::default(),
                    QString::default(),
                    QString::default(),
                );

                #[cfg(not(target_os = "macos"))]
                match result {
                    crate::diagnose::AirPlayRequirement::Ready => t.requirementsChecked(
                        true,
                        QString::default(),
                        QString::default(),
                        QString::default(),
                    ),
                    crate::diagnose::AirPlayRequirement::NeedsAction {
                        dependency_id,
                        reason,
                    } => t.requirementsChecked(
                        false,
                        QString::from(dependency_id),
                        QString::from(reason),
                        QString::default(),
                    ),
                    crate::diagnose::AirPlayRequirement::UnableToCheck { detail } => t
                        .requirementsChecked(
                            false,
                            QString::default(),
                            QString::from("unable_to_check"),
                            QString::from(detail),
                        ),
                }
            });
        });
    }

    fn load_gst_gl(&self) -> bool {
        crate::utils::force_load_gst_gl()
    }

    fn init(&self, video_item: QVariant) -> bool {
        AIRPLAY_QT_THREAD.get_or_init(|| self.qt_thread());

        let ptr = crate::utils::qvariant_to_ptr(video_item);

        VIDEO_ITEM_PTR.store(ptr as usize, Ordering::Release);
        unsafe {
            set_uxplay_gl_callbacks(
                rust_uxplay_connection_cb,
                rust_uxplay_get_video_item,
                rust_uxplay_connection_details_cb,
            );
        }

        std::thread::spawn(|| {
            let args = crate::settings_manager::airplay_uxplay_args();
            debug!("Starting uxplay with args: {:?}", args);

            let c_strings: Vec<CString> = args
                .into_iter()
                .filter_map(|arg| CString::new(arg).ok())
                .collect();
            let mut c_args: Vec<*mut c_char> = c_strings
                .iter()
                .map(|arg| arg.as_ptr() as *mut c_char)
                .collect();
            c_args.push(std::ptr::null_mut());

            let result = unsafe { init_uxplay((c_args.len() - 1) as i32, c_args.as_mut_ptr()) };
            if result != 0 {
                error!("uxplay failed with exit code {result}");
                if let Some(q_thread) = AIRPLAY_QT_THREAD.get() {
                    q_thread.queue(move |t| {
                        t.backendFailed(
                            result,
                            QString::from(format!(
                                "The AirPlay backend exited with code {result}."
                            )),
                        );
                    });
                }
            }
        });
        true
    }

    fn cleanup(&self) {
        unsafe {
            uxplay_cleanup();
        }
    }

    fn set_master_volume(&self, volume: f64) {
        let volume = volume.clamp(0.0, 1.0);
        debug!("Setting AirPlay master volume to {:.0}%", volume * 100.0);
        unsafe {
            uxplay_set_audio_volume(volume);
        }
    }

    fn launch_arguments(&self) -> QStringList {
        let mut arguments = QStringList::default();
        for argument in crate::settings_manager::airplay_uxplay_args() {
            arguments.push(QString::from(argument));
        }
        arguments
    }
}
