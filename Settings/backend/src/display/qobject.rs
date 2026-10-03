use super::state::{DisplayState, RequestState};
use crate::typhon::client::{ClientWorker, WorkerEvent};
use crate::typhon::output::{FeatureState, OutputSnapshot};
use crate::typhon::protocol::{ControlRequest, ControlSuccess, ProtocolOutcome};
use cxx_qt::{CxxQtType, Threading};
use cxx_qt_lib::{QList, QMap, QMapPair_QString_QVariant, QString, QVariant};
use std::pin::Pin;

#[cxx_qt::bridge]
pub mod qobject {
    unsafe extern "C++" {
        include!("cxx-qt-lib/qstring.h");
        include!("cxx-qt-lib/qvariant.h");
        include!("cxx-qt-lib/core/qlist/qlist_QVariant.h");
        type QString = cxx_qt_lib::QString;
        type QVariant = cxx_qt_lib::QVariant;
        type QList_QVariant = cxx_qt_lib::QList<QVariant>;
    }

    extern "RustQt" {
        #[qobject]
        #[qproperty(bool, available, READ = available, NOTIFY = availability_changed)]
        #[qproperty(bool, busy, READ = busy, NOTIFY = busy_changed)]
        #[qproperty(QList_QVariant, outputs, READ = outputs, NOTIFY = snapshot_changed)]
        #[qproperty(u32, output_count, cxx_name = "outputCount", READ = output_count, NOTIFY = snapshot_changed)]
        #[qproperty(bool, truncated, READ = truncated, NOTIFY = snapshot_changed)]
        #[qproperty(QString, last_error, cxx_name = "lastError", READ = last_error, NOTIFY = error_changed)]
        type SettingsDisplayController = super::SettingsDisplayControllerRust;
    }

    unsafe extern "RustQt" {
        fn available(self: &SettingsDisplayController) -> bool;
        fn busy(self: &SettingsDisplayController) -> bool;
        fn outputs(self: &SettingsDisplayController) -> QList_QVariant;
        #[cxx_name = "outputCount"]
        fn output_count(self: &SettingsDisplayController) -> u32;
        fn truncated(self: &SettingsDisplayController) -> bool;
        #[cxx_name = "lastError"]
        fn last_error(self: &SettingsDisplayController) -> QString;

        #[qsignal]
        #[cxx_name = "availabilityChanged"]
        fn availability_changed(self: Pin<&mut SettingsDisplayController>);
        #[qsignal]
        #[cxx_name = "busyChanged"]
        fn busy_changed(self: Pin<&mut SettingsDisplayController>);
        #[qsignal]
        #[cxx_name = "snapshotChanged"]
        fn snapshot_changed(self: Pin<&mut SettingsDisplayController>);
        #[qsignal]
        #[cxx_name = "errorChanged"]
        fn error_changed(self: Pin<&mut SettingsDisplayController>);

        #[qinvokable]
        fn refresh(self: Pin<&mut SettingsDisplayController>);
    }

    impl cxx_qt::Initialize for SettingsDisplayController {}
    impl cxx_qt::Threading for SettingsDisplayController {}
}

#[derive(Default)]
pub struct SettingsDisplayControllerRust {
    state: DisplayState,
    requests: RequestState,
    worker: Option<ClientWorker>,
    busy: bool,
}

impl qobject::SettingsDisplayController {
    fn available(&self) -> bool {
        self.rust().state.available()
    }
    fn busy(&self) -> bool {
        self.rust().busy
    }
    fn output_count(&self) -> u32 {
        self.rust().state.total()
    }
    fn truncated(&self) -> bool {
        self.rust().state.truncated()
    }
    fn last_error(&self) -> QString {
        QString::from(self.rust().state.last_error())
    }

    fn outputs(&self) -> qobject::QList_QVariant {
        let mut outputs = QList::default();
        for output in self.rust().state.outputs() {
            outputs.append(QVariant::from(&output_map(output)));
        }
        outputs
    }

    fn refresh(mut self: Pin<&mut Self>) {
        self.as_mut().start_request();
    }

    fn start_request(mut self: Pin<&mut Self>) {
        if self.rust().busy {
            return;
        }
        let (id, was_busy, worker_available) = {
            let mut rust = self.as_mut().rust_mut();
            let was_busy = rust.busy;
            let id = rust.requests.start();
            rust.busy = true;
            (id, was_busy, rust.worker.is_some())
        };
        if !was_busy {
            self.as_mut().busy_changed();
        }
        if !worker_available {
            self.as_mut()
                .complete_failure(id, String::from("Typhon control: worker is unavailable"));
            return;
        }
        let submitted = self.rust().worker.as_ref().is_some_and(|worker| {
            worker
                .submit(
                    id,
                    ControlRequest::Output(crate::typhon::output::OutputRequest::Get),
                )
                .is_ok()
        });
        if !submitted {
            self.as_mut()
                .complete_failure(id, String::from("Typhon control: worker is unavailable"));
        }
    }

    fn handle_worker_event(mut self: Pin<&mut Self>, event: WorkerEvent) {
        let WorkerEvent::RequestFinished { id, result } = event else {
            return;
        };
        if !self.rust().requests.is_current(id) {
            return;
        }
        match *result {
            Ok(ProtocolOutcome::Success(ControlSuccess::Outputs(snapshot))) => {
                let was_available = self.rust().state.available();
                let had_error = !self.rust().state.last_error().is_empty();
                let result = self.as_mut().rust_mut().state.apply(snapshot);
                if let Err(error) = result {
                    self.as_mut().complete_failure(id, error);
                    return;
                }
                self.as_mut().rust_mut().requests.finish(id);
                if self.rust().busy {
                    self.as_mut().rust_mut().busy = false;
                    self.as_mut().busy_changed();
                }
                if !was_available {
                    self.as_mut().availability_changed();
                }
                self.as_mut().snapshot_changed();
                if had_error {
                    self.as_mut().error_changed();
                }
            }
            Ok(ProtocolOutcome::Success(_)) => self.as_mut().complete_failure(
                id,
                String::from("Typhon returned an unexpected result to Display Settings."),
            ),
            Ok(ProtocolOutcome::ServerRejected(error)) => {
                self.as_mut().complete_failure(id, error.message)
            }
            Err(error) => self.as_mut().complete_failure(id, error.to_string()),
        }
    }

    fn complete_failure(mut self: Pin<&mut Self>, id: u64, message: String) {
        // Request ids protect state publication; a failed stale request has no effects either.
        if !self.rust().requests.is_current(id) {
            return;
        }
        let was_available = self.rust().state.available();
        let was_busy = self.rust().busy;
        let error_changed = self.rust().state.last_error() != message;
        self.as_mut().rust_mut().requests.finish(id);
        self.as_mut().rust_mut().state.fail(message.clone());
        if was_available {
            self.as_mut().availability_changed();
        }
        self.as_mut().snapshot_changed();
        if error_changed {
            self.as_mut().error_changed();
        }
        if was_busy {
            self.as_mut().rust_mut().busy = false;
            self.as_mut().busy_changed();
        }
    }
}

impl cxx_qt::Initialize for qobject::SettingsDisplayController {
    fn initialize(mut self: Pin<&mut Self>) {
        let qt_thread = self.qt_thread();
        let worker = ClientWorker::new(move |event| {
            let _ = qt_thread.queue(move |object| object.handle_worker_event(event));
        });
        self.as_mut().rust_mut().worker = worker.ok();
        if self.rust().worker.is_none() {
            self.as_mut()
                .rust_mut()
                .state
                .fail(String::from("Typhon control: worker is unavailable"));
        }
    }
}

fn output_map(output: &OutputSnapshot) -> QMap<QMapPair_QString_QVariant> {
    let mut map = QMap::default();
    put_string(&mut map, "id", &output.id);
    put_string(&mut map, "name", &output.name);
    put_optional_string(&mut map, "make", output.make.as_deref());
    put_optional_string(&mut map, "model", output.model.as_deref());
    put_optional_string(&mut map, "serial", output.serial.as_deref());
    put_bool(&mut map, "enabled", output.enabled);
    put_bool(&mut map, "hasCurrentMode", output.current_mode.is_some());
    put_u32(
        &mut map,
        "currentWidth",
        output.current_mode.as_ref().map_or(0, |mode| mode.width),
    );
    put_u32(
        &mut map,
        "currentHeight",
        output.current_mode.as_ref().map_or(0, |mode| mode.height),
    );
    put_u32(
        &mut map,
        "currentRefreshMilliHz",
        output
            .current_mode
            .as_ref()
            .map_or(0, |mode| mode.refresh_millihz),
    );
    put_bool(
        &mut map,
        "hasPhysicalSize",
        output.physical_size_mm.is_some(),
    );
    put_u32(
        &mut map,
        "physicalWidthMm",
        output
            .physical_size_mm
            .as_ref()
            .map_or(0, |size| size.width_mm),
    );
    put_u32(
        &mut map,
        "physicalHeightMm",
        output
            .physical_size_mm
            .as_ref()
            .map_or(0, |size| size.height_mm),
    );
    put_u32(&mut map, "scaleMilli", output.scale_milli);
    put_string(&mut map, "transform", &output.transform);
    put_i32(&mut map, "x", output.position.x);
    put_i32(&mut map, "y", output.position.y);
    put_bool(&mut map, "focused", output.focused);
    put_string(&mut map, "vrrState", feature_state(output.vrr.state));
    put_string(&mut map, "backend", &output.backend);
    put_bool(&mut map, "modesTruncated", output.modes_truncated);
    let mut modes = QList::default();
    for mode in &output.modes {
        let mut mode_map = QMap::default();
        put_u32(&mut mode_map, "width", mode.width);
        put_u32(&mut mode_map, "height", mode.height);
        put_u32(&mut mode_map, "refreshMilliHz", mode.refresh_millihz);
        put_bool(&mut mode_map, "preferred", mode.preferred);
        put_bool(&mut mode_map, "interlaced", mode.interlaced);
        modes.append(QVariant::from(&mode_map));
    }
    map.insert(QString::from("modes"), QVariant::from(&modes));
    map
}

fn feature_state(state: FeatureState) -> &'static str {
    match state {
        FeatureState::Unavailable => "unavailable",
        FeatureState::Configured => "configured",
        FeatureState::Available => "available",
        FeatureState::Active => "active",
        FeatureState::Degraded => "degraded",
    }
}

fn put_string(map: &mut QMap<QMapPair_QString_QVariant>, key: &str, value: &str) {
    map.insert(QString::from(key), QVariant::from(&QString::from(value)));
}
fn put_optional_string(map: &mut QMap<QMapPair_QString_QVariant>, key: &str, value: Option<&str>) {
    map.insert(
        QString::from(key),
        value.map_or_else(QVariant::default, |text| {
            QVariant::from(&QString::from(text))
        }),
    );
}
fn put_bool(map: &mut QMap<QMapPair_QString_QVariant>, key: &str, value: bool) {
    map.insert(QString::from(key), QVariant::from(&value));
}
fn put_i32(map: &mut QMap<QMapPair_QString_QVariant>, key: &str, value: i32) {
    map.insert(QString::from(key), QVariant::from(&value));
}
fn put_u32(map: &mut QMap<QMapPair_QString_QVariant>, key: &str, value: u32) {
    map.insert(QString::from(key), QVariant::from(&value));
}
