use crate::typhon::output::{
    OutputConfigureRequest, OutputListSnapshot, OutputSnapshot, OutputTransactionRequest,
    OutputTransactionSnapshot, OutputTransform,
};
use std::collections::HashMap;

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct OutputDraft {
    pub output_id: String,
    pub base_configuration_generation: u64,
    pub mode_id: u32,
    pub scale_milli: u32,
    pub transform: OutputTransform,
}

impl OutputDraft {
    pub fn differs_from(&self, output: &OutputSnapshot) -> bool {
        output
            .current_mode
            .as_ref()
            .is_none_or(|mode| mode.id != self.mode_id)
            || output.scale_milli != self.scale_milli
            || output.transform != self.transform
    }

    pub fn is_stale(&self, output: &OutputSnapshot) -> bool {
        self.output_id != output.id
            || self.base_configuration_generation != output.configuration_generation
    }

    pub fn values_supported(&self, output: &OutputSnapshot) -> bool {
        if self.is_stale(output) || output.current_mode.is_none() {
            return false;
        }
        let mode_is_current = output
            .current_mode
            .as_ref()
            .is_some_and(|mode| mode.id == self.mode_id);
        let mode_exists = output.modes.iter().any(|mode| mode.id == self.mode_id);
        let mode_supported = mode_is_current
            || (output.mutation_capabilities.mode_selection_supported && mode_exists);
        let scale_supported = output.scale_milli == self.scale_milli
            || output
                .mutation_capabilities
                .scale
                .supports(self.scale_milli);
        let transform_supported = output.transform == self.transform
            || output
                .mutation_capabilities
                .transform
                .supports(self.transform);
        mode_supported && scale_supported && transform_supported
    }

    pub fn can_apply(
        &self,
        output: &OutputSnapshot,
        transaction: Option<&OutputTransactionSnapshot>,
    ) -> bool {
        transaction.is_none()
            && !self.is_stale(output)
            && self.differs_from(output)
            && self.values_supported(output)
    }

    pub fn configure_request(&self) -> OutputConfigureRequest {
        OutputConfigureRequest {
            version: 1,
            output_id: self.output_id.clone(),
            base_configuration_generation: self.base_configuration_generation,
            mode_id: self.mode_id,
            scale_milli: self.scale_milli,
            transform: self.transform,
        }
    }
}

#[derive(Default)]
pub struct DisplayState {
    available: bool,
    outputs: Vec<OutputSnapshot>,
    total: u32,
    truncated: bool,
    transaction: Option<OutputTransactionSnapshot>,
    drafts: HashMap<String, OutputDraft>,
    last_error: String,
}

impl DisplayState {
    pub fn available(&self) -> bool {
        self.available
    }
    pub fn outputs(&self) -> &[OutputSnapshot] {
        &self.outputs
    }
    pub fn total(&self) -> u32 {
        self.total
    }
    pub fn truncated(&self) -> bool {
        self.truncated
    }
    pub fn transaction(&self) -> Option<&OutputTransactionSnapshot> {
        self.transaction.as_ref()
    }
    pub fn draft(&self, output_id: &str) -> Option<&OutputDraft> {
        self.drafts.get(output_id)
    }
    pub fn last_error(&self) -> &str {
        &self.last_error
    }

    pub fn apply(&mut self, snapshot: OutputListSnapshot) -> Result<(), String> {
        snapshot
            .validate()
            .map_err(|_| String::from("Typhon returned an invalid display snapshot."))?;

        let prior_outputs: HashMap<&str, &OutputSnapshot> = self
            .outputs
            .iter()
            .map(|output| (output.id.as_str(), output))
            .collect();
        let mut reconciled = HashMap::new();
        let server_transaction = snapshot.transaction.as_ref();
        for output in &snapshot.outputs {
            let actual = draft_from_output(output);
            let draft = if server_transaction.is_some() {
                actual
            } else if let Some(existing) = self.drafts.get(&output.id) {
                match prior_outputs.get(output.id.as_str()) {
                    // A clean draft follows authoritative changes. An edited draft is kept on
                    // its old base generation so it becomes visibly stale and cannot be applied.
                    Some(previous)
                        if !existing.differs_from(previous)
                            && existing.base_configuration_generation
                                == previous.configuration_generation =>
                    {
                        actual
                    }
                    _ => Some(existing.clone()),
                }
            } else {
                actual
            };
            if let Some(draft) = draft {
                reconciled.insert(output.id.clone(), draft);
            }
        }
        self.outputs = snapshot.outputs;
        self.total = snapshot.total;
        self.truncated = snapshot.truncated;
        self.transaction = snapshot.transaction;
        self.drafts = reconciled;
        self.available = true;
        self.last_error.clear();
        Ok(())
    }

    pub fn set_draft(
        &mut self,
        output_id: &str,
        mode_id: u32,
        scale_milli: u32,
        transform: OutputTransform,
    ) -> Result<(), String> {
        let output = self
            .outputs
            .iter()
            .find(|output| output.id == output_id)
            .ok_or_else(|| String::from("The selected output is no longer available."))?;
        if mode_id == 0 || scale_milli == 0 {
            return Err(String::from("Display draft values must be positive."));
        }
        let draft = self
            .drafts
            .get_mut(output_id)
            .ok_or_else(|| String::from("The selected output has no current mode."))?;
        if draft.base_configuration_generation != output.configuration_generation {
            return Err(String::from(
                "The display draft is based on a stale configuration.",
            ));
        }
        draft.mode_id = mode_id;
        draft.scale_milli = scale_milli;
        draft.transform = transform;
        Ok(())
    }

    pub fn can_apply(&self, output_id: &str) -> bool {
        if !self.available {
            return false;
        }
        let Some(output) = self.outputs.iter().find(|output| output.id == output_id) else {
            return false;
        };
        self.drafts
            .get(output_id)
            .is_some_and(|draft| draft.can_apply(output, self.transaction.as_ref()))
    }

    pub fn confirm_request(&self) -> Option<OutputTransactionRequest> {
        self.transaction
            .as_ref()
            .filter(|transaction| {
                transaction.state
                    == crate::typhon::output::OutputTransactionState::PendingConfirmation
            })
            .map(|transaction| OutputTransactionRequest {
                version: 1,
                transaction_id: transaction.id,
            })
    }

    pub fn revert_request(&self) -> Option<OutputTransactionRequest> {
        self.transaction
            .as_ref()
            .filter(|transaction| {
                matches!(
                    transaction.state,
                    crate::typhon::output::OutputTransactionState::PendingConfirmation
                        | crate::typhon::output::OutputTransactionState::PersistencePending
                )
            })
            .map(|transaction| OutputTransactionRequest {
                version: 1,
                transaction_id: transaction.id,
            })
    }

    pub fn mutation_rejected(&mut self, message: String) {
        self.last_error = message;
    }

    pub fn fail(&mut self, error: String) {
        self.available = false;
        self.outputs.clear();
        self.total = 0;
        self.truncated = false;
        self.transaction = None;
        self.drafts.clear();
        self.last_error = error;
    }
}

fn draft_from_output(output: &OutputSnapshot) -> Option<OutputDraft> {
    Some(OutputDraft {
        output_id: output.id.clone(),
        base_configuration_generation: output.configuration_generation,
        mode_id: output.current_mode.as_ref()?.id,
        scale_milli: output.scale_milli,
        transform: output.transform,
    })
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum RequestPurpose {
    Refresh,
    Configure,
    Confirm,
    Revert,
}

impl RequestPurpose {
    pub fn server_rejection_invalidates_authority(self) -> bool {
        self == Self::Refresh
    }
}

#[derive(Default)]
pub struct RequestState {
    next_id: u64,
    active: Option<(u64, RequestPurpose)>,
}

impl RequestState {
    pub fn start(&mut self, purpose: RequestPurpose) -> u64 {
        let id = self.next_id.max(1);
        self.next_id = id.wrapping_add(1).max(1);
        self.active = Some((id, purpose));
        id
    }

    pub fn finish(&mut self, id: u64) -> bool {
        if self.active.map(|active| active.0) != Some(id) {
            return false;
        }
        self.active = None;
        true
    }

    pub fn is_current(&self, id: u64) -> bool {
        self.active.map(|active| active.0) == Some(id)
    }

    pub fn purpose(&self, id: u64) -> Option<RequestPurpose> {
        self.active
            .filter(|active| active.0 == id)
            .map(|active| active.1)
    }
}

#[cfg(test)]
mod tests {
    use super::{DisplayState, OutputDraft, RequestPurpose, RequestState};
    use crate::typhon::output::{
        FeatureState, FeatureStateSnapshot, ModeSnapshot, OutputListSnapshot, OutputModeSnapshot,
        OutputMutationCapabilities, OutputSnapshot, OutputTransform, PositionSnapshot,
        ScaleMutationCapability, TransformMutationCapability,
    };

    fn output() -> OutputSnapshot {
        OutputSnapshot {
            id: String::from("output-1"),
            name: String::from("DP-1"),
            make: None,
            model: None,
            serial: None,
            enabled: true,
            current_mode: Some(ModeSnapshot {
                id: 1,
                width: 1920,
                height: 1080,
                refresh_millihz: 165_000,
            }),
            physical_size_mm: None,
            configuration_generation: 1,
            scale_milli: 1000,
            transform: OutputTransform::Normal,
            mutation_capabilities: OutputMutationCapabilities {
                mode_selection_supported: true,
                scale: ScaleMutationCapability {
                    supported: false,
                    min_milli: None,
                    max_milli: None,
                    values_milli: Vec::new(),
                },
                transform: TransformMutationCapability {
                    supported: false,
                    transforms: Vec::new(),
                },
                enable_disable_supported: false,
                topology_supported: false,
                vrr_mutation_supported: false,
            },
            position: PositionSnapshot { x: 0, y: 0 },
            focused: true,
            backend: String::from("atomic"),
            vrr: FeatureStateSnapshot {
                state: FeatureState::Available,
            },
            direct_scanout: FeatureStateSnapshot {
                state: FeatureState::Unavailable,
            },
            modes: vec![
                OutputModeSnapshot {
                    id: 1,
                    width: 1920,
                    height: 1080,
                    refresh_millihz: 165_000,
                    preferred: true,
                    interlaced: false,
                },
                OutputModeSnapshot {
                    id: 2,
                    width: 1920,
                    height: 1080,
                    refresh_millihz: 60_000,
                    preferred: false,
                    interlaced: false,
                },
            ],
            modes_truncated: false,
        }
    }

    fn snapshot() -> OutputListSnapshot {
        OutputListSnapshot {
            outputs: vec![output()],
            total: 1,
            truncated: false,
            transaction: None,
        }
    }

    #[test]
    fn display_controller_state_starts_unavailable_and_empty() {
        let state = DisplayState::default();
        assert!(!state.available());
        assert!(state.outputs().is_empty());
    }

    #[test]
    fn successful_snapshot_replaces_outputs_and_clears_error() {
        let mut state = DisplayState::default();
        state.fail(String::from("connection failed"));
        state.apply(snapshot()).expect("valid snapshot publishes");
        assert!(state.available());
        assert_eq!(state.outputs().len(), 1);
        assert!(state.last_error().is_empty());
    }

    #[test]
    fn failed_refresh_clears_stale_output_and_retry_can_recover() {
        let mut state = DisplayState::default();
        state.apply(snapshot()).expect("valid snapshot publishes");
        state.fail(String::from("Typhon rejected the request"));
        assert!(!state.available());
        assert!(state.outputs().is_empty());
        assert_eq!(state.last_error(), "Typhon rejected the request");
        state
            .apply(snapshot())
            .expect("retry can publish current state");
        assert!(state.available());
        assert_eq!(state.outputs().len(), 1);
        assert!(state.last_error().is_empty());
    }

    #[test]
    fn stale_display_completion_is_ignored_after_a_newer_request_starts() {
        let mut requests = RequestState::default();
        let old = requests.start(RequestPurpose::Refresh);
        let current = requests.start(RequestPurpose::Configure);
        assert!(!requests.finish(old));
        assert_eq!(requests.purpose(current), Some(RequestPurpose::Configure));
        assert!(requests.finish(current));
    }

    #[test]
    fn only_rejected_authoritative_refresh_invalidates_display_state() {
        for purpose in [
            RequestPurpose::Configure,
            RequestPurpose::Confirm,
            RequestPurpose::Revert,
        ] {
            assert!(!purpose.server_rejection_invalidates_authority());
        }
        assert!(RequestPurpose::Refresh.server_rejection_invalidates_authority());
    }

    #[test]
    fn mutation_rejection_preserves_authoritative_state_and_availability() {
        let mut state = DisplayState::default();
        state.apply(snapshot()).unwrap();
        state.mutation_rejected(String::from("output_test_failed"));
        assert!(state.available());
        assert_eq!(state.outputs().len(), 1);
        assert_eq!(state.last_error(), "output_test_failed");
    }

    #[test]
    fn edited_draft_remains_based_on_old_generation_after_new_snapshot() {
        let mut state = DisplayState::default();
        state.apply(snapshot()).unwrap();
        state
            .set_draft("output-1", 2, 1000, OutputTransform::Normal)
            .unwrap();
        let mut newer = snapshot();
        newer.outputs[0].configuration_generation = 2;
        let mode = newer.outputs[0].current_mode.as_mut().unwrap();
        mode.id = 2;
        mode.refresh_millihz = 60_000;
        state.apply(newer).unwrap();
        let draft = state.draft("output-1").unwrap();
        assert_eq!(draft.base_configuration_generation, 1);
        assert!(!state.can_apply("output-1"));
    }

    #[test]
    fn clean_draft_reconciles_to_new_configuration_generation() {
        let mut state = DisplayState::default();
        state.apply(snapshot()).unwrap();
        let mut newer = snapshot();
        newer.outputs[0].configuration_generation = 2;
        let mode = newer.outputs[0].current_mode.as_mut().unwrap();
        mode.id = 2;
        mode.refresh_millihz = 60_000;
        state.apply(newer).unwrap();
        assert_eq!(
            state
                .draft("output-1")
                .unwrap()
                .base_configuration_generation,
            2
        );
        assert_eq!(state.draft("output-1").unwrap().mode_id, 2);
    }

    #[test]
    fn active_server_transaction_replaces_local_speculation_during_refresh_recovery() {
        let mut state = DisplayState::default();
        state.apply(snapshot()).unwrap();
        state
            .set_draft("output-1", 2, 1000, OutputTransform::Normal)
            .unwrap();
        let mut active = snapshot();
        active.outputs[0].configuration_generation = 2;
        let mode = active.outputs[0].current_mode.as_mut().unwrap();
        mode.id = 2;
        mode.refresh_millihz = 60_000;
        active.transaction = Some(crate::typhon::output::OutputTransactionSnapshot {
            id: 7,
            output_id: String::from("output-1"),
            state: crate::typhon::output::OutputTransactionState::PendingConfirmation,
            applied_configuration_generation: 2,
            remaining_ms: 9000,
            error_code: None,
        });
        state.apply(active).unwrap();
        assert_eq!(state.transaction().unwrap().id, 7);
        assert_eq!(state.draft("output-1").unwrap().mode_id, 2);
        assert!(state.confirm_request().is_some());
        assert!(state.revert_request().is_some());
        assert!(!state.can_apply("output-1"));
    }

    #[test]
    fn persistence_pending_exposes_revert_but_not_duplicate_confirm() {
        let mut state = DisplayState::default();
        let mut active = snapshot();
        active.transaction = Some(crate::typhon::output::OutputTransactionSnapshot {
            id: 7,
            output_id: String::from("output-1"),
            state: crate::typhon::output::OutputTransactionState::PersistencePending,
            applied_configuration_generation: 1,
            remaining_ms: 8_000,
            error_code: None,
        });

        state.apply(active).unwrap();

        assert!(state.confirm_request().is_none());
        assert!(state.revert_request().is_some());
    }

    #[test]
    fn rolling_back_is_visible_and_exposes_no_confirmation_action() {
        let mut state = DisplayState::default();
        let mut active = snapshot();
        active.transaction = Some(crate::typhon::output::OutputTransactionSnapshot {
            id: 7,
            output_id: String::from("output-1"),
            state: crate::typhon::output::OutputTransactionState::RollingBack,
            applied_configuration_generation: 1,
            remaining_ms: 0,
            error_code: None,
        });

        state.apply(active).unwrap();

        assert!(state.confirm_request().is_none());
        assert!(state.revert_request().is_none());
    }

    #[test]
    fn transaction_actions_match_server_eligibility_for_all_phases() {
        use crate::typhon::output::OutputTransactionState;

        let cases = [
            (
                OutputTransactionState::PendingConfirmation,
                5_000,
                None,
                true,
                true,
            ),
            (
                OutputTransactionState::PersistencePending,
                4_000,
                None,
                false,
                true,
            ),
            (OutputTransactionState::RollingBack, 0, None, false, false),
            (
                OutputTransactionState::RollbackFailed,
                0,
                Some(String::from("output_rollback_failed")),
                false,
                false,
            ),
        ];

        for (phase, remaining_ms, error_code, can_confirm, can_revert) in cases {
            let mut state = DisplayState::default();
            let mut active = snapshot();
            active.transaction = Some(crate::typhon::output::OutputTransactionSnapshot {
                id: 7,
                output_id: String::from("output-1"),
                state: phase,
                applied_configuration_generation: 1,
                remaining_ms,
                error_code,
            });
            state
                .apply(active)
                .expect("each phase has a valid authoritative snapshot");
            assert_eq!(state.confirm_request().is_some(), can_confirm);
            assert_eq!(state.revert_request().is_some(), can_revert);
        }
    }

    #[test]
    fn confirmation_disconnect_recovers_server_phase_from_the_next_snapshot() {
        use crate::typhon::output::OutputTransactionState;

        let mut state = DisplayState::default();
        let mut pending = snapshot();
        pending.transaction = Some(crate::typhon::output::OutputTransactionSnapshot {
            id: 7,
            output_id: String::from("output-1"),
            state: OutputTransactionState::PendingConfirmation,
            applied_configuration_generation: 1,
            remaining_ms: 5_000,
            error_code: None,
        });
        state.apply(pending).unwrap();
        assert_eq!(state.confirm_request().unwrap().transaction_id, 7);

        state.fail(String::from("connection lost during confirmation"));
        assert!(state.transaction().is_none());
        assert!(!state.available());

        let mut reconnected = snapshot();
        reconnected.transaction = Some(crate::typhon::output::OutputTransactionSnapshot {
            id: 7,
            output_id: String::from("output-1"),
            state: OutputTransactionState::PersistencePending,
            applied_configuration_generation: 1,
            remaining_ms: 3_000,
            error_code: None,
        });
        state
            .apply(reconnected)
            .expect("reconnect refresh restores server-owned confirmation state");
        assert!(state.available());
        assert_eq!(
            state.transaction().unwrap().state,
            OutputTransactionState::PersistencePending
        );
        assert!(state.confirm_request().is_none());
        assert!(state.revert_request().is_some());
    }

    #[test]
    fn reconnect_during_rollback_recovers_terminal_phase_without_actions() {
        use crate::typhon::output::OutputTransactionState;

        let mut state = DisplayState::default();
        let mut persistence_pending = snapshot();
        persistence_pending.transaction = Some(crate::typhon::output::OutputTransactionSnapshot {
            id: 7,
            output_id: String::from("output-1"),
            state: OutputTransactionState::PersistencePending,
            applied_configuration_generation: 1,
            remaining_ms: 2_000,
            error_code: None,
        });
        state.apply(persistence_pending).unwrap();
        state.fail(String::from("connection lost during persistence"));

        let mut rolling_back = snapshot();
        rolling_back.transaction = Some(crate::typhon::output::OutputTransactionSnapshot {
            id: 7,
            output_id: String::from("output-1"),
            state: OutputTransactionState::RollingBack,
            applied_configuration_generation: 1,
            remaining_ms: 0,
            error_code: None,
        });
        state
            .apply(rolling_back)
            .expect("reconnect recovers the server-owned rollback phase");
        assert_eq!(
            state.transaction().unwrap().state,
            OutputTransactionState::RollingBack
        );
        assert!(state.confirm_request().is_none());
        assert!(state.revert_request().is_none());

        state.fail(String::from("connection lost during rollback"));
        let mut rollback_failed = snapshot();
        rollback_failed.transaction = Some(crate::typhon::output::OutputTransactionSnapshot {
            id: 7,
            output_id: String::from("output-1"),
            state: OutputTransactionState::RollbackFailed,
            applied_configuration_generation: 1,
            remaining_ms: 0,
            error_code: Some(String::from("output_rollback_failed")),
        });
        state
            .apply(rollback_failed)
            .expect("reconnect recovers a failed rollback as a terminal state");
        assert_eq!(
            state.transaction().unwrap().state,
            OutputTransactionState::RollbackFailed
        );
        assert!(state.confirm_request().is_none());
        assert!(state.revert_request().is_none());
    }

    #[test]
    fn draft_request_keeps_native_mode_id_and_generation_opaque() {
        let draft = OutputDraft {
            output_id: "output-1".into(),
            base_configuration_generation: 18,
            mode_id: 77,
            scale_milli: 1000,
            transform: OutputTransform::Normal,
        };
        let request = draft.configure_request();
        assert_eq!(request.base_configuration_generation, 18);
        assert_eq!(request.mode_id, 77);
    }
}
