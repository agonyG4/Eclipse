use std::pin::Pin;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Mutex};

use cxx_qt::{CxxQtType, Threading};
use cxx_qt_lib::{QList, QMap, QMapPair_QString_QVariant, QString, QVariant};

use super::bluez::client::BluetoothWorker;
use super::device::BluetoothDevice;
use super::engine::{CoreSnapshot, ServiceState};

#[cxx_qt::bridge]
pub mod qobject {
    unsafe extern "C++" {
        include!("cxx-qt-lib/qstring.h");
        type QString = cxx_qt_lib::QString;
        include!("cxx-qt-lib/qvariant.h");
        type QVariant = cxx_qt_lib::QVariant;
        include!("cxx-qt-lib/core/qlist/qlist_QVariant.h");
        type QList_QVariant = cxx_qt_lib::QList<QVariant>;
    }

    extern "RustQt" {
        #[qobject]
        #[qproperty(i32, state, READ = state, NOTIFY = snapshot_changed)]
        #[qproperty(bool, available, READ = available, NOTIFY = snapshot_changed)]
        #[qproperty(bool, ready, READ = ready, NOTIFY = snapshot_changed)]
        #[qproperty(bool, adapter_available, cxx_name = "adapterAvailable", READ = adapter_available, NOTIFY = snapshot_changed)]
        #[qproperty(QString, adapter_path, cxx_name = "adapterPath", READ = adapter_path, NOTIFY = snapshot_changed)]
        #[qproperty(QString, adapter_name, cxx_name = "adapterName", READ = adapter_name, NOTIFY = snapshot_changed)]
        #[qproperty(bool, powered, READ = powered, NOTIFY = snapshot_changed)]
        #[qproperty(bool, power_pending, cxx_name = "powerPending", READ = power_pending, NOTIFY = snapshot_changed)]
        #[qproperty(bool, scanning, READ = scanning, NOTIFY = snapshot_changed)]
        #[qproperty(i32, connected_count, cxx_name = "connectedCount", READ = connected_count, NOTIFY = snapshot_changed)]
        #[qproperty(QString, connected_name, cxx_name = "connectedName", READ = connected_name, NOTIFY = snapshot_changed)]
        #[qproperty(QList_QVariant, devices, READ = devices, NOTIFY = snapshot_changed)]
        #[qproperty(QString, error_string, cxx_name = "errorString", READ = error_string, NOTIFY = snapshot_changed)]
        type RustBluetoothEngine = super::RustBluetoothEngineRust;
    }

    unsafe extern "RustQt" {
        fn state(self: &RustBluetoothEngine) -> i32;
        fn available(self: &RustBluetoothEngine) -> bool;
        fn ready(self: &RustBluetoothEngine) -> bool;
        #[cxx_name = "adapterAvailable"]
        fn adapter_available(self: &RustBluetoothEngine) -> bool;
        #[cxx_name = "adapterPath"]
        fn adapter_path(self: &RustBluetoothEngine) -> QString;
        #[cxx_name = "adapterName"]
        fn adapter_name(self: &RustBluetoothEngine) -> QString;
        fn powered(self: &RustBluetoothEngine) -> bool;
        #[cxx_name = "powerPending"]
        fn power_pending(self: &RustBluetoothEngine) -> bool;
        fn scanning(self: &RustBluetoothEngine) -> bool;
        #[cxx_name = "connectedCount"]
        fn connected_count(self: &RustBluetoothEngine) -> i32;
        #[cxx_name = "connectedName"]
        fn connected_name(self: &RustBluetoothEngine) -> QString;
        fn devices(self: &RustBluetoothEngine) -> QList_QVariant;
        #[cxx_name = "errorString"]
        fn error_string(self: &RustBluetoothEngine) -> QString;

        #[qsignal]
        #[cxx_name = "snapshotChanged"]
        fn snapshot_changed(self: Pin<&mut RustBluetoothEngine>);

        #[qinvokable]
        fn start(self: Pin<&mut RustBluetoothEngine>) -> bool;
        #[qinvokable]
        fn stop(self: Pin<&mut RustBluetoothEngine>);
        #[qinvokable]
        #[cxx_name = "setPowered"]
        fn set_powered(self: Pin<&mut RustBluetoothEngine>, powered: bool) -> bool;
        #[qinvokable]
        #[cxx_name = "requestScan"]
        fn request_scan(self: Pin<&mut RustBluetoothEngine>, owner: &QString) -> bool;
        #[qinvokable]
        #[cxx_name = "releaseScan"]
        fn release_scan(self: Pin<&mut RustBluetoothEngine>, owner: &QString) -> bool;
        #[qinvokable]
        #[cxx_name = "connectDevice"]
        fn connect_device(self: Pin<&mut RustBluetoothEngine>, object_path: &QString) -> bool;
        #[qinvokable]
        #[cxx_name = "disconnectDevice"]
        fn disconnect_device(self: Pin<&mut RustBluetoothEngine>, object_path: &QString) -> bool;
    }

    impl cxx_qt::Initialize for RustBluetoothEngine {}
    impl cxx_qt::Threading for RustBluetoothEngine {}
}

#[derive(Default)]
pub struct RustBluetoothEngineRust {
    worker: Option<BluetoothWorker>,
    snapshot: CoreSnapshot,
    session_generation: u64,
    running: bool,
    initialization_error: Option<String>,
}

struct SnapshotMailbox {
    latest: Mutex<Option<CoreSnapshot>>,
    queued: AtomicBool,
    qt_thread: cxx_qt::CxxQtThread<qobject::RustBluetoothEngine>,
}

impl SnapshotMailbox {
    fn post(self: &Arc<Self>, snapshot: CoreSnapshot) {
        if let Ok(mut latest) = self.latest.lock() {
            *latest = Some(snapshot);
        } else {
            return;
        }
        self.queue_if_needed();
    }

    fn queue_if_needed(self: &Arc<Self>) {
        if self.queued.swap(true, Ordering::AcqRel) {
            return;
        }
        let mailbox = Arc::clone(self);
        let queue_result = self.qt_thread.queue(move |mut object| {
            let snapshot = mailbox
                .latest
                .lock()
                .ok()
                .and_then(|mut latest| latest.take());
            if let Some(snapshot) = snapshot {
                object.as_mut().handle_worker_snapshot(snapshot);
            }
            mailbox.queued.store(false, Ordering::Release);
            let has_pending = mailbox.latest.lock().is_ok_and(|latest| latest.is_some());
            if has_pending {
                mailbox.queue_if_needed();
            }
        });
        if queue_result.is_err() {
            self.queued.store(false, Ordering::Release);
            if let Ok(mut latest) = self.latest.lock() {
                *latest = None;
            }
        }
    }
}

impl qobject::RustBluetoothEngine {
    fn state(&self) -> i32 {
        match self.rust().snapshot.state {
            ServiceState::Stopped => 0,
            ServiceState::Starting => 1,
            ServiceState::Ready => 2,
            ServiceState::Unavailable => 3,
            ServiceState::Degraded => 4,
        }
    }

    fn available(&self) -> bool {
        self.rust().snapshot.available
    }

    fn ready(&self) -> bool {
        self.rust().snapshot.ready
    }

    fn adapter_available(&self) -> bool {
        self.rust().snapshot.adapter_available
    }

    fn adapter_path(&self) -> QString {
        QString::from(&self.rust().snapshot.adapter_path)
    }

    fn adapter_name(&self) -> QString {
        QString::from(&self.rust().snapshot.adapter_name)
    }

    fn powered(&self) -> bool {
        self.rust().snapshot.powered
    }

    fn power_pending(&self) -> bool {
        self.rust().snapshot.power_pending
    }

    fn scanning(&self) -> bool {
        self.rust().snapshot.scanning
    }

    fn connected_count(&self) -> i32 {
        self.rust().snapshot.connected_count
    }

    fn connected_name(&self) -> QString {
        QString::from(&self.rust().snapshot.connected_name)
    }

    fn devices(&self) -> QList<QVariant> {
        let mut devices = QList::default();
        for device in &self.rust().snapshot.devices {
            devices.append(QVariant::from(&device_variant_map(device)));
        }
        devices
    }

    fn error_string(&self) -> QString {
        self.rust()
            .snapshot
            .error
            .as_ref()
            .map_or_else(QString::default, QString::from)
    }

    fn start(mut self: Pin<&mut Self>) -> bool {
        if self.rust().running {
            return true;
        }
        let Some(session_generation) = self.rust().session_generation.checked_add(1) else {
            self.as_mut()
                .set_initialization_error("Bluetooth service generation exhausted");
            return false;
        };
        let submitted = self
            .rust()
            .worker
            .as_ref()
            .is_some_and(|worker| worker.start(session_generation));
        if !submitted {
            self.as_mut()
                .set_initialization_error("Bluetooth backend command queue is unavailable");
            return false;
        }
        let snapshot = CoreSnapshot {
            session_generation,
            state: ServiceState::Starting,
            ..CoreSnapshot::default()
        };
        self.as_mut().rust_mut().session_generation = session_generation;
        self.as_mut().rust_mut().running = true;
        self.as_mut().rust_mut().snapshot = snapshot;
        self.as_mut().snapshot_changed();
        true
    }

    fn stop(mut self: Pin<&mut Self>) {
        if !self.rust().running {
            return;
        }
        let session_generation = self.rust().session_generation.saturating_add(1);
        if let Some(worker) = self.rust().worker.as_ref() {
            worker.stop(session_generation);
        }
        self.as_mut().rust_mut().session_generation = session_generation;
        self.as_mut().rust_mut().running = false;
        self.as_mut().rust_mut().snapshot = CoreSnapshot {
            session_generation,
            ..CoreSnapshot::default()
        };
        self.as_mut().snapshot_changed();
    }

    fn set_powered(self: Pin<&mut Self>, powered: bool) -> bool {
        self.rust().running
            && self.rust().snapshot.available
            && self.rust().snapshot.adapter_available
            && self
                .rust()
                .worker
                .as_ref()
                .is_some_and(|worker| worker.set_powered(self.rust().session_generation, powered))
    }

    fn request_scan(self: Pin<&mut Self>, owner: &QString) -> bool {
        let owner = String::from(owner);
        !owner.is_empty()
            && self.rust().running
            && self
                .rust()
                .worker
                .as_ref()
                .is_some_and(|worker| worker.request_scan(self.rust().session_generation, owner))
    }

    fn release_scan(self: Pin<&mut Self>, owner: &QString) -> bool {
        let owner = String::from(owner);
        if !self.rust().running {
            return false;
        }
        if let Some(worker) = self.rust().worker.as_ref() {
            worker.release_scan(self.rust().session_generation, owner);
            return true;
        }
        false
    }

    fn connect_device(self: Pin<&mut Self>, object_path: &QString) -> bool {
        let object_path = String::from(object_path);
        self.rust().snapshot.can_connect(&object_path)
            && self.rust().running
            && self.rust().worker.as_ref().is_some_and(|worker| {
                worker.connect(self.rust().session_generation, object_path, true)
            })
    }

    fn disconnect_device(self: Pin<&mut Self>, object_path: &QString) -> bool {
        let object_path = String::from(object_path);
        !object_path.is_empty()
            && self.rust().running
            && self.rust().worker.as_ref().is_some_and(|worker| {
                worker.connect(self.rust().session_generation, object_path, false)
            })
    }

    fn handle_worker_snapshot(mut self: Pin<&mut Self>, snapshot: CoreSnapshot) {
        if !self.rust().running || snapshot.session_generation != self.rust().session_generation {
            return;
        }
        self.as_mut().rust_mut().snapshot = snapshot;
        self.as_mut().snapshot_changed();
    }

    fn set_initialization_error(mut self: Pin<&mut Self>, error: &str) {
        let error = String::from(error);
        self.as_mut().rust_mut().initialization_error = Some(error.clone());
        self.as_mut().rust_mut().snapshot = CoreSnapshot {
            state: ServiceState::Unavailable,
            error: Some(error),
            ..CoreSnapshot::default()
        };
        self.as_mut().snapshot_changed();
    }
}

impl cxx_qt::Initialize for qobject::RustBluetoothEngine {
    fn initialize(mut self: Pin<&mut Self>) {
        let mailbox = Arc::new(SnapshotMailbox {
            latest: Mutex::new(None),
            queued: AtomicBool::new(false),
            qt_thread: self.qt_thread(),
        });
        let callback_mailbox = Arc::clone(&mailbox);
        match BluetoothWorker::spawn(move |snapshot| callback_mailbox.post(snapshot)) {
            Ok(worker) => self.as_mut().rust_mut().worker = Some(worker),
            Err(error) => {
                self.set_initialization_error(&format!("Bluetooth worker failed to start: {error}"))
            }
        }
    }
}

fn device_variant_map(device: &BluetoothDevice) -> QMap<QMapPair_QString_QVariant> {
    let mut map = QMap::default();
    insert_string(&mut map, "id", &device.id);
    insert_string(&mut map, "objectPath", &device.object_path);
    insert_string(&mut map, "address", &device.address);
    insert_string(&mut map, "name", &device.name);
    insert_bool(&mut map, "paired", device.paired);
    insert_bool(&mut map, "trusted", device.trusted);
    insert_bool(&mut map, "connected", device.connected);
    insert_bool(&mut map, "discovered", device.discovered);
    insert_string(&mut map, "icon", &device.icon);
    map.insert(QString::from("rssi"), QVariant::from(&device.rssi));
    map.insert(
        QString::from("batteryPercent"),
        QVariant::from(&device.battery_percent),
    );
    map
}

fn insert_string(map: &mut QMap<QMapPair_QString_QVariant>, key: &str, value: &str) {
    map.insert(QString::from(key), QVariant::from(&QString::from(value)));
}

fn insert_bool(map: &mut QMap<QMapPair_QString_QVariant>, key: &str, value: bool) {
    map.insert(QString::from(key), QVariant::from(&value));
}
