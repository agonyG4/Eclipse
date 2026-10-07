use super::state::{DisplayState, RequestPurpose, RequestState};
use crate::typhon::client::{ClientWorker, WorkerEvent};
use crate::typhon::output::{FeatureState, OutputRequest, OutputSnapshot, OutputTransform};
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
        #[qproperty(bool, transaction_active, cxx_name = "transactionActive", READ = transaction_active, NOTIFY = snapshot_changed)]
        #[qproperty(bool, confirmation_pending, cxx_name = "confirmationPending", READ = confirmation_pending, NOTIFY = snapshot_changed)]
        #[qproperty(u64, transaction_id, cxx_name = "transactionId", READ = transaction_id, NOTIFY = snapshot_changed)]
        #[qproperty(u32, transaction_remaining_ms, cxx_name = "transactionRemainingMs", READ = transaction_remaining_ms, NOTIFY = snapshot_changed)]
        #[qproperty(QString, transaction_output_id, cxx_name = "transactionOutputId", READ = transaction_output_id, NOTIFY = snapshot_changed)]
        #[qproperty(QString, transaction_state, cxx_name = "transactionState", READ = transaction_state, NOTIFY = snapshot_changed)]
        #[qproperty(QString, transaction_error_code, cxx_name = "transactionErrorCode", READ = transaction_error_code, NOTIFY = snapshot_changed)]
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
        #[cxx_name = "transactionActive"]
        fn transaction_active(self: &SettingsDisplayController) -> bool;
        #[cxx_name = "confirmationPending"]
        fn confirmation_pending(self: &SettingsDisplayController) -> bool;
        #[cxx_name = "transactionId"]
        fn transaction_id(self: &SettingsDisplayController) -> u64;
        #[cxx_name = "transactionRemainingMs"]
        fn transaction_remaining_ms(self: &SettingsDisplayController) -> u32;
        #[cxx_name = "transactionOutputId"]
        fn transaction_output_id(self: &SettingsDisplayController) -> QString;
        #[cxx_name = "transactionState"]
        fn transaction_state(self: &SettingsDisplayController) -> QString;
        #[cxx_name = "transactionErrorCode"]
        fn transaction_error_code(self: &SettingsDisplayController) -> QString;

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
        #[qinvokable]
        #[cxx_name = "setDraft"]
        fn set_draft(
            self: Pin<&mut SettingsDisplayController>,
            output_id: QString,
            mode_id: u32,
            scale_milli: u32,
            transform: QString,
        ) -> bool;
        #[qinvokable]
        #[cxx_name = "applyDraft"]
        fn apply_draft(self: Pin<&mut SettingsDisplayController>, output_id: QString);
        #[qinvokable]
        fn confirm(self: Pin<&mut SettingsDisplayController>);
        #[qinvokable]
        fn revert(self: Pin<&mut SettingsDisplayController>);
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
    fn transaction_active(&self) -> bool {
        self.rust().state.transaction().is_some()
    }
    fn confirmation_pending(&self) -> bool {
        self.rust().state.confirm_request().is_some()
    }
    fn transaction_id(&self) -> u64 {
        self.rust()
            .state
            .transaction()
            .map_or(0, |transaction| transaction.id)
    }
    fn transaction_remaining_ms(&self) -> u32 {
        self.rust()
            .state
            .transaction()
            .map_or(0, |transaction| transaction.remaining_ms)
    }
    fn transaction_output_id(&self) -> QString {
        QString::from(
            self.rust()
                .state
                .transaction()
                .map_or("", |transaction| transaction.output_id.as_str()),
        )
    }
    fn transaction_state(&self) -> QString {
        let value = self
            .rust()
            .state
            .transaction()
            .map_or("none", |transaction| match transaction.state {
                crate::typhon::output::OutputTransactionState::PendingConfirmation => {
                    "pending_confirmation"
                }
                crate::typhon::output::OutputTransactionState::PersistencePending => {
                    "persistence_pending"
                }
                crate::typhon::output::OutputTransactionState::RollingBack => "rolling_back",
                crate::typhon::output::OutputTransactionState::RollbackFailed => "rollback_failed",
            });
        QString::from(value)
    }
    fn transaction_error_code(&self) -> QString {
        QString::from(
            self.rust()
                .state
                .transaction()
                .and_then(|transaction| transaction.error_code.as_deref())
                .unwrap_or(""),
        )
    }

    fn outputs(&self) -> qobject::QList_QVariant {
        let mut outputs = QList::default();
        for output in self.rust().state.outputs() {
            outputs.append(QVariant::from(&output_map(
                output,
                self.rust().state.draft(&output.id),
                self.rust().state.can_apply(&output.id),
            )));
        }
        outputs
    }

    fn refresh(mut self: Pin<&mut Self>) {
        self.as_mut()
            .start_request(RequestPurpose::Refresh, OutputRequest::Get);
    }

    fn set_draft(
        mut self: Pin<&mut Self>,
        output_id: QString,
        mode_id: u32,
        scale_milli: u32,
        transform: QString,
    ) -> bool {
        let Some(transform) = parse_transform(&transform.to_string()) else {
            self.as_mut()
                .set_local_error(String::from("Unknown display transform."));
            return false;
        };
        let result = self.as_mut().rust_mut().state.set_draft(
            &output_id.to_string(),
            mode_id,
            scale_milli,
            transform,
        );
        match result {
            Ok(()) => {
                self.as_mut().snapshot_changed();
                true
            }
            Err(error) => {
                self.as_mut().set_local_error(error);
                false
            }
        }
    }

    fn apply_draft(mut self: Pin<&mut Self>, output_id: QString) {
        let output_id = output_id.to_string();
        if !self.rust().state.can_apply(&output_id) {
            self.as_mut()
                .set_local_error(String::from("The current display draft cannot be applied."));
            return;
        }
        let Some(draft) = self.rust().state.draft(&output_id) else {
            return;
        };
        let request = OutputRequest::Configure(draft.configure_request());
        self.as_mut()
            .start_request(RequestPurpose::Configure, request);
    }

    fn confirm(mut self: Pin<&mut Self>) {
        let Some(transaction) = self.rust().state.confirm_request() else {
            self.as_mut()
                .set_local_error(String::from("There is no pending display confirmation."));
            return;
        };
        self.as_mut()
            .start_request(RequestPurpose::Confirm, OutputRequest::Confirm(transaction));
    }

    fn revert(mut self: Pin<&mut Self>) {
        let Some(transaction) = self.rust().state.revert_request() else {
            self.as_mut()
                .set_local_error(String::from("There is no pending display confirmation."));
            return;
        };
        self.as_mut()
            .start_request(RequestPurpose::Revert, OutputRequest::Revert(transaction));
    }

    fn start_request(mut self: Pin<&mut Self>, purpose: RequestPurpose, request: OutputRequest) {
        if self.rust().busy {
            return;
        }
        let (id, was_busy, worker_available) = {
            let mut rust = self.as_mut().rust_mut();
            let was_busy = rust.busy;
            let id = rust.requests.start(purpose);
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
        let submitted = self
            .rust()
            .worker
            .as_ref()
            .is_some_and(|worker| worker.submit(id, ControlRequest::Output(request)).is_ok());
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
            Ok(ProtocolOutcome::ServerRejected(error)) => match self.rust().requests.purpose(id) {
                Some(purpose) if purpose.server_rejection_invalidates_authority() => {
                    self.as_mut().complete_failure(id, error.message)
                }
                Some(_) => self.as_mut().complete_mutation_rejection(id, error.message),
                None => {}
            },
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

    fn complete_mutation_rejection(mut self: Pin<&mut Self>, id: u64, message: String) {
        if !self.rust().requests.is_current(id) {
            return;
        }
        let was_busy = self.rust().busy;
        let error_changed = self.rust().state.last_error() != message;
        self.as_mut().rust_mut().requests.finish(id);
        self.as_mut().rust_mut().state.mutation_rejected(message);
        if error_changed {
            self.as_mut().error_changed();
        }
        if was_busy {
            self.as_mut().rust_mut().busy = false;
            self.as_mut().busy_changed();
        }
    }

    fn set_local_error(mut self: Pin<&mut Self>, message: String) {
        let changed = self.rust().state.last_error() != message;
        self.as_mut().rust_mut().state.mutation_rejected(message);
        if changed {
            self.as_mut().error_changed();
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

fn output_map(
    output: &OutputSnapshot,
    draft: Option<&super::state::OutputDraft>,
    can_apply: bool,
) -> QMap<QMapPair_QString_QVariant> {
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
        "currentModeId",
        output.current_mode.as_ref().map_or(0, |mode| mode.id),
    );
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
    put_string(&mut map, "transform", transform_name(output.transform));
    put_u64(
        &mut map,
        "configurationGeneration",
        output.configuration_generation,
    );
    put_bool(
        &mut map,
        "modeSelectionSupported",
        output.mutation_capabilities.mode_selection_supported,
    );
    put_bool(
        &mut map,
        "scaleMutationSupported",
        output.mutation_capabilities.scale.supported,
    );
    put_optional_u32(
        &mut map,
        "scaleMinMilli",
        output.mutation_capabilities.scale.min_milli,
    );
    put_optional_u32(
        &mut map,
        "scaleMaxMilli",
        output.mutation_capabilities.scale.max_milli,
    );
    put_bool(
        &mut map,
        "transformMutationSupported",
        output.mutation_capabilities.transform.supported,
    );
    put_bool(
        &mut map,
        "enableDisableSupported",
        output.mutation_capabilities.enable_disable_supported,
    );
    put_bool(
        &mut map,
        "topologySupported",
        output.mutation_capabilities.topology_supported,
    );
    put_bool(
        &mut map,
        "vrrMutationSupported",
        output.mutation_capabilities.vrr_mutation_supported,
    );
    put_bool(&mut map, "draftPresent", draft.is_some());
    put_u32(
        &mut map,
        "draftModeId",
        draft.map_or(0, |draft| draft.mode_id),
    );
    put_u32(
        &mut map,
        "draftScaleMilli",
        draft.map_or(0, |draft| draft.scale_milli),
    );
    put_string(
        &mut map,
        "draftTransform",
        draft.map_or("normal", |draft| transform_name(draft.transform)),
    );
    put_bool(
        &mut map,
        "draftStale",
        draft.is_some_and(|draft| draft.is_stale(output)),
    );
    put_bool(
        &mut map,
        "draftDiffers",
        draft.is_some_and(|draft| draft.differs_from(output)),
    );
    put_bool(
        &mut map,
        "draftValuesSupported",
        draft.is_some_and(|draft| draft.values_supported(output)),
    );
    put_bool(&mut map, "canApplyDraft", can_apply);
    put_u64(
        &mut map,
        "draftConfigurationGeneration",
        draft.map_or(0, |draft| draft.base_configuration_generation),
    );
    let mut scale_values = QList::default();
    for value in &output.mutation_capabilities.scale.values_milli {
        scale_values.append(QVariant::from(value));
    }
    map.insert(
        QString::from("scaleValuesMilli"),
        QVariant::from(&scale_values),
    );
    let mut transforms = QList::default();
    for transform in &output.mutation_capabilities.transform.transforms {
        transforms.append(QVariant::from(&QString::from(transform_name(*transform))));
    }
    map.insert(
        QString::from("supportedTransforms"),
        QVariant::from(&transforms),
    );
    put_i32(&mut map, "x", output.position.x);
    put_i32(&mut map, "y", output.position.y);
    put_bool(&mut map, "focused", output.focused);
    put_string(&mut map, "vrrState", feature_state(output.vrr.state));
    put_string(&mut map, "backend", &output.backend);
    put_bool(&mut map, "modesTruncated", output.modes_truncated);
    let mut modes = QList::default();
    for mode in &output.modes {
        let mut mode_map = QMap::default();
        put_u32(&mut mode_map, "id", mode.id);
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

fn parse_transform(value: &str) -> Option<OutputTransform> {
    match value {
        "normal" => Some(OutputTransform::Normal),
        "rotate90" => Some(OutputTransform::Rotate90),
        "rotate180" => Some(OutputTransform::Rotate180),
        "rotate270" => Some(OutputTransform::Rotate270),
        "flipped" => Some(OutputTransform::Flipped),
        "flipped90" => Some(OutputTransform::Flipped90),
        "flipped180" => Some(OutputTransform::Flipped180),
        "flipped270" => Some(OutputTransform::Flipped270),
        _ => None,
    }
}

fn transform_name(transform: OutputTransform) -> &'static str {
    match transform {
        OutputTransform::Normal => "normal",
        OutputTransform::Rotate90 => "rotate90",
        OutputTransform::Rotate180 => "rotate180",
        OutputTransform::Rotate270 => "rotate270",
        OutputTransform::Flipped => "flipped",
        OutputTransform::Flipped90 => "flipped90",
        OutputTransform::Flipped180 => "flipped180",
        OutputTransform::Flipped270 => "flipped270",
    }
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
fn put_u64(map: &mut QMap<QMapPair_QString_QVariant>, key: &str, value: u64) {
    map.insert(QString::from(key), QVariant::from(&value));
}
fn put_optional_u32(map: &mut QMap<QMapPair_QString_QVariant>, key: &str, value: Option<u32>) {
    map.insert(
        QString::from(key),
        value.map_or_else(QVariant::default, |value| QVariant::from(&value)),
    );
}
