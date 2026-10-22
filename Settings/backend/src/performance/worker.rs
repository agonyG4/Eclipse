use astrea_gaming::{
    command::NativeCommandRunner, performance::PerformanceService, store::config_home,
};
use serde_json::Value;
use std::sync::{
    Arc,
    mpsc::{self, SyncSender},
};
use std::thread;

#[derive(Debug)]
pub struct PerformanceWorkerEvent {
    pub generation: u64,
    pub snapshot: Option<Value>,
    pub error: Option<String>,
}

enum Request {
    Refresh {
        generation: u64,
    },
    SetPreference {
        generation: u64,
        key: String,
        value: Value,
    },
    ApplyProfile {
        generation: u64,
        profile: String,
    },
    ApplyScheduler {
        generation: u64,
        scheduler: String,
        mode: String,
    },
}

pub struct PerformanceWorker {
    sender: SyncSender<Request>,
}

impl PerformanceWorker {
    pub fn new<F>(callback: F) -> Result<Self, String>
    where
        F: Fn(PerformanceWorkerEvent) + Send + 'static,
    {
        let (sender, receiver) = mpsc::sync_channel(1);
        let service =
            config_home().map(|home| PerformanceService::new(home, Arc::new(NativeCommandRunner)));
        thread::Builder::new()
            .name(String::from("astrea-settings-performance"))
            .spawn(move || {
                while let Ok(request) = receiver.recv() {
                    let generation = match &request {
                        Request::Refresh { generation }
                        | Request::SetPreference { generation, .. }
                        | Request::ApplyProfile { generation, .. }
                        | Request::ApplyScheduler { generation, .. } => *generation,
                    };
                    let result = match service.as_ref() {
                        Ok(service) => execute(service, request),
                        Err(error) => Err((error.clone(), None)),
                    };
                    let event = match result {
                        Ok(snapshot) => PerformanceWorkerEvent {
                            generation,
                            snapshot: Some(snapshot),
                            error: None,
                        },
                        Err((error, snapshot)) => PerformanceWorkerEvent {
                            generation,
                            snapshot,
                            error: Some(error),
                        },
                    };
                    callback(event);
                }
            })
            .map_err(|error| format!("failed to start Settings Performance worker: {error}"))?;
        Ok(Self { sender })
    }

    pub fn refresh(&self, generation: u64) -> Result<(), String> {
        self.send(Request::Refresh { generation })
    }

    pub fn set_preference(&self, generation: u64, key: String, value: Value) -> Result<(), String> {
        self.send(Request::SetPreference {
            generation,
            key,
            value,
        })
    }

    pub fn apply_profile(&self, generation: u64, profile: String) -> Result<(), String> {
        self.send(Request::ApplyProfile {
            generation,
            profile,
        })
    }

    pub fn apply_scheduler(
        &self,
        generation: u64,
        scheduler: String,
        mode: String,
    ) -> Result<(), String> {
        self.send(Request::ApplyScheduler {
            generation,
            scheduler,
            mode,
        })
    }

    fn send(&self, request: Request) -> Result<(), String> {
        self.sender
            .try_send(request)
            .map_err(|error| format!("Settings Performance worker is unavailable: {error}"))
    }
}

fn execute(
    service: &PerformanceService,
    request: Request,
) -> Result<Value, (String, Option<Value>)> {
    let result = match request {
        Request::Refresh { .. } => return service.snapshot().map_err(|error| (error, None)),
        Request::SetPreference { key, value, .. } => service.set_preference(&key, value),
        Request::ApplyProfile { profile, .. } => service.apply_profile(&profile),
        Request::ApplyScheduler {
            scheduler, mode, ..
        } => service.apply_scheduler(&scheduler, &mode),
    };
    result.map_err(|error| (error, service.snapshot().ok()))
}
