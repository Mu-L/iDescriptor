// SPDX-FileCopyrightText: 2025-2026 Uncore <https://github.com/uncor3>
// SPDX-License-Identifier: AGPL-3.0-or-later

use crate::qt_threading::QtThreading;
use crate::{RUNTIME, qvariantmap_insert};
use macros::QtThreading;
use qmetaobject::prelude::*;
use qttypes::{QStringList, QVariantMap};
use std::collections::HashMap;
use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, Mutex};
use std::time::Duration;
#[cfg(target_os = "linux")]
use izeroconf::AvahiMdnsBrowser;
#[cfg(any(target_os = "windows", target_os = "macos"))]
use izeroconf::BonjourMdnsBrowser;
use izeroconf::prelude::{TEventLoop, TMdnsBrowser, TTxtRecord};
use izeroconf::{
    BrowserEvent, DeviceMetadataResolution, DiscoveryBackend, PureRustMdnsBrowser,
    ServiceDiscovery, ServiceType,
};

const STATE_LOADING: i32 = 0;
const STATE_STARTED: i32 = 1;
const STATE_FAILED: i32 = 2;

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
enum BackendSelection {
    #[default]
    Auto,
    Backend(DiscoveryBackend),
}

impl BackendSelection {
    fn as_str(self) -> &'static str {
        match self {
            Self::Auto => "auto",
            Self::Backend(backend) => backend.as_str(),
        }
    }
}

trait DiscoveryBackendName {
    fn as_str(self) -> &'static str;
}

impl DiscoveryBackendName for DiscoveryBackend {
    fn as_str(self) -> &'static str {
        match self {
            Self::PureRust => "pure_rust",
            #[cfg(target_os = "linux")]
            Self::Avahi => "avahi",
            #[cfg(any(target_os = "windows", target_os = "macos"))]
            Self::Bonjour => "bonjour",
        }
    }
}

fn parse_backend_selection(value: &str) -> anyhow::Result<BackendSelection> {
    match value.trim() {
        "auto" => Ok(BackendSelection::Auto),
        "pure_rust" => Ok(BackendSelection::Backend(DiscoveryBackend::PureRust)),
        #[cfg(target_os = "linux")]
        "avahi" => Ok(BackendSelection::Backend(DiscoveryBackend::Avahi)),
        #[cfg(any(target_os = "windows", target_os = "macos"))]
        "bonjour" => Ok(BackendSelection::Backend(DiscoveryBackend::Bonjour)),
        value => anyhow::bail!("unsupported network discovery backend: {value}"),
    }
}

#[derive(Clone)]
struct BrowseContext {
    generation: u64,
    generation_guard: Arc<AtomicU64>,
    q_thread: crate::qt_threading::QtThread<NetworkDeviceProvider>,
    backend_error: Arc<Mutex<Option<String>>>,
}

impl BrowseContext {
    fn is_current(&self) -> bool {
        self.generation_guard.load(Ordering::SeqCst) == self.generation
    }
}

#[derive(Clone, Debug)]
struct NetworkDevice {
    instance_name: String,
    name: String,
    hostname: String,
    address: String,
    port: u16,
    mac_address: String,
    product_type: Option<String>,
    product_version: Option<String>,
    build_version: Option<String>,
}

impl NetworkDevice {
    fn from_discovery(discovery: &ServiceDiscovery) -> Option<Self> {
        let metadata = discovery.device_metadata().as_ref();
        println!("{:?}", metadata);
        let txt = discovery.txt().as_ref();
        let instance_name = discovery.name().clone();
        let fallback_mac = instance_name
            .split('@')
            .next()
            .unwrap_or_default()
            .to_string();
        let mac_address = metadata
            .and_then(|value| value.wifi_address().clone())
            .filter(|value| !value.is_empty())
            .unwrap_or(fallback_mac);
        if mac_address.is_empty() || discovery.address().is_empty() {
            return None;
        }

        let trimmed_host = discovery.host_name().trim_end_matches('.');
        let friendly_host = trimmed_host
            .strip_suffix(".local")
            .unwrap_or(trimmed_host)
            .to_string();
        let name = metadata
            .and_then(|value| value.device_name().clone())
            .or_else(|| txt.and_then(|value| value.get("DvNm")))
            .or_else(|| txt.and_then(|value| value.get("Name")))
            .filter(|value| !value.is_empty())
            .unwrap_or(friendly_host);

        Some(Self {
            instance_name,
            name,
            hostname: discovery.host_name().clone(),
            address: discovery.address().clone(),
            port: *discovery.port(),
            mac_address,
            product_type: metadata.and_then(|value| value.product_type().clone()),
            product_version: metadata.and_then(|value| value.product_version().clone()),
            build_version: metadata.and_then(|value| value.build_version().clone()),
        })
    }

    fn to_variant_map(&self) -> QVariantMap {
        let mut map = QVariantMap::default();
        qvariantmap_insert!(map, "name", QString::from(self.name.clone()));
        qvariantmap_insert!(map, "address", QString::from(self.address.clone()));
        qvariantmap_insert!(map, "port", u32::from(self.port));
        qvariantmap_insert!(map, "macAddress", QString::from(self.mac_address.clone()));
        qvariantmap_insert!(map, "hostname", QString::from(self.hostname.clone()));
        if let Some(value) = &self.product_type {
            qvariantmap_insert!(map, "productType", QString::from(value.clone()));
        }
        if let Some(value) = &self.product_version {
            qvariantmap_insert!(map, "productVersion", QString::from(value.clone()));
        }
        if let Some(value) = &self.build_version {
            qvariantmap_insert!(map, "buildVersion", QString::from(value.clone()));
        }
        map
    }
}

#[allow(non_snake_case)]
#[derive(QObject, QtThreading)]
pub struct NetworkDeviceProvider {
    base: qt_base_class!(trait QObject),
    state: qt_property!(i32; NOTIFY stateChanged),
    stateChanged: qt_signal!(),
    configured_backend: qt_property!(QString; NOTIFY configuredBackendChanged),
    configuredBackendChanged: qt_signal!(),
    active_backend: qt_property!(QString; NOTIFY activeBackendChanged),
    activeBackendChanged: qt_signal!(),
    available_backends: qt_property!(QStringList; CONST),
    localNetworkPrivacyRequired: qt_property!(bool; CONST),
    Loading: qt_property!(i32; CONST),
    Started: qt_property!(i32; CONST),
    Failed: qt_property!(i32; CONST),
    started: qt_signal!(),
    failed: qt_signal!(message: QString),
    deviceAdded: qt_signal!(device: QVariantMap),
    deviceRemoved: qt_signal!(deviceName: QString),
    startBrowsing: qt_method!(fn(&mut self)),
    restartBrowsing: qt_method!(fn(&mut self)),
    set_backend: qt_method!(fn(&mut self, value: QString)),
    getNetworkDevices: qt_method!(fn(&self) -> QVariantMap),
    getNetworkDeviceByMac: qt_method!(fn(&self, macAddress: QString) -> QVariantMap),
    devices: HashMap<String, NetworkDevice>,
    generation: Arc<AtomicU64>,
}

impl Default for NetworkDeviceProvider {
    fn default() -> Self {
        Self::new(QString::from("auto"))
    }
}

impl NetworkDeviceProvider {
    pub fn new(configured_backend: QString) -> Self {
        let selection =
            parse_backend_selection(&configured_backend.to_string()).unwrap_or_else(|error| {
                log::warn!("invalid saved network discovery backend, using auto: {error}");
                BackendSelection::Auto
            });
        Self {
            base: Default::default(),
            state: STATE_LOADING,
            stateChanged: Default::default(),
            configured_backend: QString::from(selection.as_str()),
            configuredBackendChanged: Default::default(),
            active_backend: QString::default(),
            activeBackendChanged: Default::default(),
            available_backends: available_backends(),
            localNetworkPrivacyRequired: crate::native::local_network_privacy_required(),
            Loading: STATE_LOADING,
            Started: STATE_STARTED,
            Failed: STATE_FAILED,
            started: Default::default(),
            failed: Default::default(),
            deviceAdded: Default::default(),
            deviceRemoved: Default::default(),
            startBrowsing: Default::default(),
            restartBrowsing: Default::default(),
            set_backend: Default::default(),
            getNetworkDevices: Default::default(),
            getNetworkDeviceByMac: Default::default(),
            devices: HashMap::new(),
            generation: Arc::new(AtomicU64::new(0)),
        }
    }
}

fn available_backends() -> QStringList {
    let mut backends = QStringList::default();
    backends.push(QString::from("auto"));
    backends.push(QString::from("pure_rust"));
    #[cfg(target_os = "linux")]
    backends.push(QString::from("avahi"));
    #[cfg(any(target_os = "windows", target_os = "macos"))]
    backends.push(QString::from("bonjour"));
    backends
}

fn configure_browser_callback<B: TMdnsBrowser>(browser: &mut B, context: BrowseContext) {
    browser.set_service_callback(Box::new(move |result, _callback_context| {
        if !context.is_current() {
            return;
        }
        match result {
            Ok(BrowserEvent::Add(discovery)) => {
                if let Some(device) = NetworkDevice::from_discovery(&discovery) {
                    let generation = context.generation;
                    context.q_thread.queue(move |provider| {
                        if provider.generation.load(Ordering::SeqCst) == generation {
                            provider.add_device(device);
                        }
                    });
                }
            }
            Ok(BrowserEvent::Remove(removal)) => {
                let generation = context.generation;
                let instance_name = removal.name().clone();
                context.q_thread.queue(move |provider| {
                    if provider.generation.load(Ordering::SeqCst) == generation {
                        provider.remove_device(&instance_name);
                    }
                });
            }
            Err(error) => {
                let message = error.to_string();
                log::warn!("mDNS service event failed: {message}");
                *context
                    .backend_error
                    .lock()
                    .expect("backend error lock poisoned") = Some(message);
            }
        }
    }));
}

fn run_browser<B: TMdnsBrowser>(mut browser: B, context: BrowseContext) -> anyhow::Result<()> {
    *context
        .backend_error
        .lock()
        .expect("backend error lock poisoned") = None;
    configure_browser_callback(&mut browser, context.clone());
    let event_loop = browser.browse_services()?;
    let backend = B::backend();
    let generation = context.generation;
    context.q_thread.queue(move |provider| {
        if provider.generation.load(Ordering::SeqCst) == generation {
            provider.set_active_backend(Some(backend));
            provider.set_state(STATE_STARTED);
            provider.started();
        }
    });

    while context.is_current() {
        event_loop.poll(Duration::from_millis(250))?;
        if let Some(error) = context
            .backend_error
            .lock()
            .expect("backend error lock poisoned")
            .take()
        {
            anyhow::bail!(error);
        }
    }
    Ok(())
}

fn run_pure_rust(service_type: ServiceType, context: BrowseContext) -> anyhow::Result<()> {
    let mut browser = PureRustMdnsBrowser::new(service_type);
    browser.set_device_metadata_resolution(DeviceMetadataResolution::AppleMobile {
        timeout: Duration::from_secs(2),
    });
    run_browser(browser, context)
}

#[cfg(target_os = "linux")]
fn run_avahi(service_type: ServiceType, context: BrowseContext) -> anyhow::Result<()> {
    let mut browser = AvahiMdnsBrowser::new(service_type);
    browser.set_device_metadata_resolution(DeviceMetadataResolution::AppleMobile {
        timeout: Duration::from_secs(2),
    });
    run_browser(browser, context)
}

#[cfg(any(target_os = "windows", target_os = "macos"))]
fn run_bonjour(service_type: ServiceType, context: BrowseContext) -> anyhow::Result<()> {
    let mut browser = BonjourMdnsBrowser::new(service_type);
    browser.set_device_metadata_resolution(DeviceMetadataResolution::AppleMobile {
        timeout: Duration::from_secs(2),
    });
    run_browser(browser, context)
}

fn run_selection(
    selection: BackendSelection,
    service_type: ServiceType,
    context: BrowseContext,
) -> anyhow::Result<()> {
    match selection {
        BackendSelection::Backend(DiscoveryBackend::PureRust) => {
            run_pure_rust(service_type, context)
        }
        #[cfg(target_os = "linux")]
        BackendSelection::Backend(DiscoveryBackend::Avahi) => run_avahi(service_type, context),
        #[cfg(any(target_os = "windows", target_os = "macos"))]
        BackendSelection::Backend(DiscoveryBackend::Bonjour) => run_bonjour(service_type, context),
        BackendSelection::Auto => {
            #[cfg(target_os = "linux")]
            let native_result = run_avahi(service_type.clone(), context.clone());
            #[cfg(any(target_os = "windows", target_os = "macos"))]
            let native_result = run_bonjour(service_type.clone(), context.clone());

            match native_result {
                Ok(()) => Ok(()),
                Err(error) if context.is_current() => {
                    log::warn!("native mDNS backend unavailable, using pure-Rust backend: {error}");
                    run_pure_rust(service_type, context)
                }
                Err(error) => Err(error),
            }
        }
    }
}

#[allow(non_snake_case)]
impl NetworkDeviceProvider {
    fn startBrowsing(&mut self) {
        if self.state == STATE_STARTED {
            return;
        }
        self.start_browsing_inner();
    }

    fn restartBrowsing(&mut self) {
        self.stop_and_clear();
        self.start_browsing_inner();
    }

    fn set_backend(&mut self, value: QString) {
        let selection = match parse_backend_selection(&value.to_string()) {
            Ok(selection) => selection,
            Err(error) => {
                log::warn!("{error}");
                return;
            }
        };
        if self.configured_backend.to_string() == selection.as_str() {
            return;
        }

        self.configured_backend = QString::from(selection.as_str());
        self.configuredBackendChanged();
        crate::settings_manager::SettingsManager::set_network_discovery_backend_value(
            self.configured_backend.clone(),
        );
        self.stop_and_clear();
        self.start_browsing_inner();
    }

    fn getNetworkDevices(&self) -> QVariantMap {
        let mut devices = QVariantMap::default();
        for (mac_address, device) in &self.devices {
            qvariantmap_insert!(devices, mac_address, device.to_variant_map());
        }
        devices
    }

    fn getNetworkDeviceByMac(&self, macAddress: QString) -> QVariantMap {
        self.devices
            .get(&macAddress.to_string())
            .map(NetworkDevice::to_variant_map)
            .unwrap_or_default()
    }

    fn start_browsing_inner(&mut self) {
        let selection = match parse_backend_selection(&self.configured_backend.to_string()) {
            Ok(selection) => selection,
            Err(error) => {
                self.set_state(STATE_FAILED);
                self.failed(QString::from(error.to_string()));
                return;
            }
        };
        let generation = self.generation.fetch_add(1, Ordering::SeqCst) + 1;
        self.set_state(STATE_LOADING);
        self.set_active_backend(None);
        let generation_guard = self.generation.clone();
        let q_thread = self.qt_thread();
        let context = BrowseContext {
            generation,
            generation_guard,
            q_thread,
            backend_error: Arc::new(Mutex::new(None)),
        };

        // no need to explicitly kill the task
        // the task will end when the generation changes
        RUNTIME.spawn(async move {
            let task = tokio::task::spawn_blocking(move || {
                let service_type = match ServiceType::new("apple-mobdev2", "tcp") {
                    Ok(service_type) => service_type,
                    Err(error) => {
                        queue_failure(&context, error.to_string());
                        return;
                    }
                };
                if let Err(error) = run_selection(selection, service_type, context.clone()) {
                    queue_failure(&context, error.to_string());
                }
            })
            .await;
            if let Err(error) = task {
                log::error!("network discovery worker failed: {}", error);
            }
        });
    }

    fn stop_and_clear(&mut self) {
        self.generation.fetch_add(1, Ordering::SeqCst);
        self.set_active_backend(None);
        let removed = self.devices.keys().cloned().collect::<Vec<_>>();
        self.devices.clear();
        for mac_address in removed {
            self.deviceRemoved(QString::from(mac_address));
        }
    }

    fn add_device(&mut self, device: NetworkDevice) {
        let key = device.mac_address.clone();
        let changed = self.devices.get(&key).is_none_or(|existing| {
            existing.address != device.address || existing.name != device.name
        });
        self.devices.insert(key, device.clone());
        if changed {
            log::info!(
                "discovered Apple network device {} at {}:{}",
                device.name,
                device.address,
                device.port
            );
            self.deviceAdded(device.to_variant_map());
        }
    }

    fn remove_device(&mut self, instance_name: &str) {
        let key = self
            .devices
            .iter()
            .find_map(|(key, device)| {
                (device.instance_name == instance_name).then_some(key.clone())
            })
            .unwrap_or_else(|| {
                instance_name
                    .split('@')
                    .next()
                    .unwrap_or(instance_name)
                    .to_string()
            });
        if self.devices.remove(&key).is_some() {
            self.deviceRemoved(QString::from(key));
        }
    }

    fn set_state(&mut self, state: i32) {
        if self.state != state {
            self.state = state;
            self.stateChanged();
        }
    }

    fn set_active_backend(&mut self, backend: Option<DiscoveryBackend>) {
        let value = QString::from(
            backend
                .map(DiscoveryBackendName::as_str)
                .unwrap_or_default(),
        );
        if self.active_backend != value {
            self.active_backend = value;
            self.activeBackendChanged();
        }
    }
}

fn queue_failure(context: &BrowseContext, message: String) {
    if !context.is_current() {
        return;
    }
    log::error!("mDNS browsing failed: {message}");
    let generation = context.generation;
    context.q_thread.queue(move |provider| {
        if provider.generation.load(Ordering::SeqCst) == generation {
            provider.set_active_backend(None);
            provider.set_state(STATE_FAILED);
            provider.failed(QString::from(message));
        }
    });
}
