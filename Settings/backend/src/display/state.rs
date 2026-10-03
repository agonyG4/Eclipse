use crate::typhon::output::{OutputListSnapshot, OutputSnapshot};

#[derive(Default)]
pub struct DisplayState {
    available: bool,
    outputs: Vec<OutputSnapshot>,
    total: u32,
    truncated: bool,
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
    pub fn last_error(&self) -> &str {
        &self.last_error
    }

    pub fn apply(&mut self, snapshot: OutputListSnapshot) -> Result<(), String> {
        snapshot
            .validate()
            .map_err(|_| String::from("Typhon returned an invalid display snapshot."))?;
        self.outputs = snapshot.outputs;
        self.total = snapshot.total;
        self.truncated = snapshot.truncated;
        self.available = true;
        self.last_error.clear();
        Ok(())
    }

    pub fn fail(&mut self, error: String) {
        self.available = false;
        self.outputs.clear();
        self.total = 0;
        self.truncated = false;
        self.last_error = error;
    }
}

#[derive(Default)]
pub struct RequestState {
    next_id: u64,
    active_id: Option<u64>,
}

impl RequestState {
    pub fn start(&mut self) -> u64 {
        let id = self.next_id.max(1);
        self.next_id = id.wrapping_add(1).max(1);
        self.active_id = Some(id);
        id
    }

    pub fn finish(&mut self, id: u64) -> bool {
        if self.active_id != Some(id) {
            return false;
        }
        self.active_id = None;
        true
    }

    pub fn is_current(&self, id: u64) -> bool {
        self.active_id == Some(id)
    }
}

#[cfg(test)]
mod tests {
    use super::{DisplayState, RequestState};
    use crate::typhon::output::{
        FeatureState, FeatureStateSnapshot, OutputListSnapshot, OutputSnapshot, PositionSnapshot,
    };

    fn output() -> OutputSnapshot {
        OutputSnapshot {
            id: String::from("output-1"),
            name: String::from("DP-1"),
            make: None,
            model: None,
            serial: None,
            enabled: true,
            current_mode: None,
            physical_size_mm: None,
            scale_milli: 1000,
            transform: String::from("normal"),
            position: PositionSnapshot { x: 0, y: 0 },
            focused: true,
            backend: String::from("atomic"),
            vrr: FeatureStateSnapshot {
                state: FeatureState::Available,
            },
            direct_scanout: FeatureStateSnapshot {
                state: FeatureState::Unavailable,
            },
            modes: Vec::new(),
            modes_truncated: false,
        }
    }

    fn snapshot() -> OutputListSnapshot {
        OutputListSnapshot {
            outputs: vec![output()],
            total: 1,
            truncated: false,
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
        let old = requests.start();
        let current = requests.start();
        assert!(!requests.finish(old));
        assert!(requests.finish(current));
    }
}
