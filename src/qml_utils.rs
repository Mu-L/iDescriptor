// SPDX-FileCopyrightText: 2025-2026 Uncore <https://github.com/uncor3>
// SPDX-License-Identifier: AGPL-3.0-or-later

use crate::device_db;
use anyhow::{Result, anyhow};
use cpp::cpp;
use log::{debug, info, warn};
use qmetaobject::{QJSValue, prelude::*};
use std::{
    ffi::c_void,
    path::{Path, PathBuf},
};

cpp! {{
    #include <QCoreApplication>
    #include <QGuiApplication>
    #include <QQmlEngine>
    #include <QString>
    #include <QStringList>
    #include <QTranslator>
}}

#[derive(QObject, Default)]
pub struct QmlUtils {
    base: qt_base_class!(trait QObject),
    engine_ptr: Option<*mut c_void>,
    get_lockdown_dir: qt_method!(fn(&self) -> QString),
    generate_uuid: qt_method!(fn(&self) -> QString),
    url_to_path: qt_method!(fn(&self, location: QString) -> QString),
    join_path: qt_method!(fn(&self, base: QString, child: QString) -> QString),
    safe_path_segment: qt_method!(fn(&self, name: QString) -> QString),
    copy_to_clipboard: qt_method!(fn(&self, text: QString)),
    get_lockdown_path: qt_method!(fn(&self) -> QString),
    set_language: qt_method!(fn(&self, lang_id: QString) -> bool),
    language_changed: qt_signal!(),
    setup_tool_window: qt_method!(fn(&self, win: QJSValue)),
    setup_main_window: qt_method!(fn(&self, win: QJSValue)),
    get_device_name: qt_method!(fn(&self, product_type: QString) -> QString),
}

impl QmlUtils {
    pub fn new(engine_ptr: *mut c_void) -> Self {
        Self {
            engine_ptr: Some(engine_ptr),
            ..Default::default()
        }
    }

    pub fn apply_language_to_engine(engine_ptr: *mut c_void, lang_id: QString) -> bool {
        if engine_ptr.is_null() {
            eprintln!("QmlUtils: engine_ptr is null, cannot apply language");
            return false;
        }

        cpp!(unsafe [engine_ptr as "QQmlEngine *", lang_id as "QString"] -> bool as "bool" {
            static QTranslator translator;

            QString normalized = lang_id.trimmed().toLower();
            if (normalized.isEmpty() || normalized == QStringLiteral("english")) {
                normalized = QStringLiteral("en");
            } else if (normalized == QStringLiteral("german")) {
                normalized = QStringLiteral("de");
            } else if (normalized == QStringLiteral("traditional chinese")
                       || normalized == QStringLiteral("zh-tw")
                       || normalized == QStringLiteral("zh_tw")
                       || normalized == QStringLiteral("zh-hant")
                       || normalized == QStringLiteral("zh_hant")
                       || normalized == QStringLiteral("zh-hk")
                       || normalized == QStringLiteral("zh_hk")
                       || normalized == QStringLiteral("zh-mo")
                       || normalized == QStringLiteral("zh_mo")) {
                normalized = QStringLiteral("zh_TW");
            } else if (normalized == QStringLiteral("chinese")
                       || normalized == QStringLiteral("simplified chinese")
                       || normalized == QStringLiteral("zh")
                       || normalized == QStringLiteral("zh-cn")
                       || normalized == QStringLiteral("zh_cn")
                       || normalized == QStringLiteral("zh-hans")) {
                normalized = QStringLiteral("zh_CN");
            } else {
                normalized.replace(QChar('-'), QChar('_'));
                const QString languageCode = normalized.section(QChar('_'), 0, 0);
                static const QStringList supportedLanguages = {
                    QStringLiteral("af"), QStringLiteral("ar"), QStringLiteral("ca"),
                    QStringLiteral("cs"), QStringLiteral("da"), QStringLiteral("de"),
                    QStringLiteral("el"), QStringLiteral("en"), QStringLiteral("es"),
                    QStringLiteral("fi"), QStringLiteral("fr"), QStringLiteral("he"),
                    QStringLiteral("hu"), QStringLiteral("it"), QStringLiteral("ja"),
                    QStringLiteral("ko"), QStringLiteral("nl"), QStringLiteral("no"),
                    QStringLiteral("pl"), QStringLiteral("pt"), QStringLiteral("ro"),
                    QStringLiteral("ru"), QStringLiteral("sr"), QStringLiteral("sv"),
                    QStringLiteral("tr"), QStringLiteral("uk"), QStringLiteral("vi")
                };
                normalized = supportedLanguages.contains(languageCode)
                    ? languageCode
                    : QStringLiteral("en");
            }

            QCoreApplication::removeTranslator(&translator);

            if (normalized != QStringLiteral("en")) {
                if (!translator.load(QStringLiteral(":/translations/") + normalized + QStringLiteral(".qm"))) {
                    engine_ptr->retranslate();
                    return false;
                }

                qApp->setLayoutDirection(
                    (normalized == QStringLiteral("ar")
                        || normalized == QStringLiteral("fa")
                        || normalized == QStringLiteral("he"))
                        ? Qt::RightToLeft
                        : Qt::LeftToRight
                );
                QCoreApplication::installTranslator(&translator);
            } else {
                qApp->setLayoutDirection(Qt::LeftToRight);
            }

            engine_ptr->retranslate();
            return true;
        })
    }

    fn get_lockdown_dir(&self) -> QString {
        QString::from(crate::utils::get_lockdown_path().to_str().unwrap())
    }

    fn generate_uuid(&self) -> QString {
        QString::from(uuid::Uuid::new_v4().to_string())
    }

    fn url_to_path(&self, location: QString) -> QString {
        let location = location.to_string();
        match url_to_pathbuf(&location) {
            Ok(path) => QString::from(path.to_string_lossy().to_string()),
            Err(err) => {
                warn!("QmlUtils failed to convert URL to path: location={location}: {err}");
                QString::from(location)
            }
        }
    }

    fn join_path(&self, base: QString, child: QString) -> QString {
        QString::from(
            PathBuf::from(base.to_string())
                .join(child.to_string())
                .to_string_lossy()
                .to_string(),
        )
    }

    fn safe_path_segment(&self, name: QString) -> QString {
        let sanitized: String = name
            .to_string()
            .chars()
            .map(|ch| match ch {
                '/' | '\\' | ':' | '*' | '?' | '"' | '<' | '>' | '|' => '_',
                ch if ch.is_control() => '_',
                ch => ch,
            })
            .collect::<String>()
            .trim()
            .trim_matches('.')
            .to_string();

        if sanitized.is_empty() {
            QString::from("Album")
        } else {
            QString::from(sanitized)
        }
    }

    fn copy_to_clipboard(&self, text: QString) {
        crate::utils::copy_to_clipboard(text);
    }

    fn get_lockdown_path(&self) -> QString {
        QString::from(
            crate::utils::get_lockdown_path()
                .to_string_lossy()
                .to_string(),
        )
    }

    fn set_language(&self, lang_id: QString) -> bool {
        if self.engine_ptr.is_none() {
            debug!("QmlUtils: engine_ptr is none, cannot set_language");
            return false;
        };
        let applied = Self::apply_language_to_engine(self.engine_ptr.unwrap(), lang_id);
        self.language_changed();
        applied
    }

    fn setup_tool_window(&self, win: QJSValue) {
        if !cfg!(target_os = "macos") {
            info!("setup_tool_window: not on macOS, skipping");
            return;
        }

        let win_id = crate::utils::get_window_id(win);

        crate::platform::macos::apply_tool_frame(win_id);
    }

    fn setup_main_window(&self, win: QJSValue) {
        if !cfg!(target_os = "macos") {
            info!("setup_main_window: not on macOS, skipping");
            return;
        }

        let win_id = crate::utils::get_window_id(win);

        crate::platform::macos::apply_main_window(win_id);
    }

    fn get_device_name(&self, product_type: QString) -> QString {
        QString::from(
            device_db::find_by_identifier(&product_type.to_string())
                .unwrap_or(&device_db::UNKNOWN_DEVICE)
                .marketing_name,
        )
    }
}

fn url_to_pathbuf(mut url: &str) -> Result<PathBuf> {
    Ok(if url.contains("://") {
        url::Url::parse(url)
            .map_err(|err| anyhow!("invalid URL {url}: {err}"))?
            .to_file_path()
            .map_err(|_| anyhow!("URL is not a local file path: {url}"))?
    } else {
        // Windows extended path
        if url.starts_with("//?/") {
            url = &url[4..];
        }
        Path::new(url).to_path_buf()
    })
}
