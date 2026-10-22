//! Performance preferences and read-only system status for Settings.

use crate::{Result, command::CommandRunner, store::JsonStore};
use serde_json::{Map, Value, json};
use std::{path::PathBuf, sync::Arc};

fn default_config() -> [(&'static str, Value); 9] {
    [
        ("profile", json!("balanced")),
        ("auto_apply", json!(true)),
        ("prefer_gamemode", json!(true)),
        ("launch_boost", json!(true)),
        ("reduce_effects", json!(false)),
        ("limit_background_tasks", json!(false)),
        ("show_status_badges", json!(true)),
        ("scx_scheduler", json!("bpfland")),
        ("scx_mode", json!("gaming")),
    ]
}

const PROFILE_IDS: &[&str] = &["power-saver", "balanced", "performance"];

#[derive(Clone)]
struct CatalogItem {
    id: &'static str,
    label: &'static str,
    best_for: &'static str,
    description: &'static str,
}
const SCHEDULERS: &[CatalogItem] = &[
    CatalogItem {
        id: "bpfland",
        label: "Bpfland",
        best_for: "Gaming, desktop, multimedia",
        description: "General interactive scheduler with cache-aware task placement. Best default for this desktop.",
    },
    CatalogItem {
        id: "lavd",
        label: "LAVD",
        best_for: "Gaming, latency-sensitive work",
        description: "Latency-aware virtual deadline scheduler. Strong choice for games and real-time-ish desktop work.",
    },
    CatalogItem {
        id: "flash",
        label: "Flash",
        best_for: "Desktop responsiveness",
        description: "A snappy general-purpose scheduler that tends to feel immediate under mixed desktop load.",
    },
    CatalogItem {
        id: "rusty",
        label: "Rusty",
        best_for: "Complex CPU topology, balanced systems",
        description: "Hybrid userspace/BPF scheduler with domain load balancing. Good for larger or asymmetrical CPUs.",
    },
    CatalogItem {
        id: "p2dq",
        label: "P2DQ",
        best_for: "Throughput with selectable modes",
        description: "Parallel queue scheduler with profiles for gaming, latency, power saving, and server workloads.",
    },
    CatalogItem {
        id: "rustland",
        label: "Rustland",
        best_for: "Experimental desktop scheduling",
        description: "Userspace-heavy scheduler. Useful for experimentation; can behave well under compile-plus-game loads.",
    },
    CatalogItem {
        id: "beerland",
        label: "Beerland",
        best_for: "Cache-heavy workloads",
        description: "Prioritizes locality and scalability. Good when keeping work near warm caches matters.",
    },
    CatalogItem {
        id: "cosmos",
        label: "Cosmos",
        best_for: "General experiments",
        description: "sched-ext scheduler available on this system; use when testing alternatives to Bpfland/LAVD.",
    },
    CatalogItem {
        id: "flow",
        label: "Flow",
        best_for: "Latency experiments",
        description: "Latency-focused scheduler family. Useful for comparing against Bpfland on stutter-sensitive workloads.",
    },
    CatalogItem {
        id: "pandemonium",
        label: "Pandemonium",
        best_for: "Advanced experimentation",
        description: "Experimental sched-ext policy for comparing non-default behavior. Keep Bpfland/LAVD as the normal desktop picks.",
    },
    CatalogItem {
        id: "cake",
        label: "Cake",
        best_for: "Tiered task behavior",
        description: "Tier/classification-oriented scheduler. Best treated as an advanced profile.",
    },
    CatalogItem {
        id: "tickless",
        label: "Tickless",
        best_for: "HPC, virtualization, server",
        description: "Server-oriented and experimental for desktop use; meant for low OS-noise workloads.",
    },
];

#[derive(Clone)]
struct ModeItem {
    id: &'static str,
    label: &'static str,
    description: &'static str,
}
const MODES: &[ModeItem] = &[
    ModeItem {
        id: "auto",
        label: "Auto",
        description: "Uses the scheduler's default policy and lets scx_loader pick the safest mode.",
    },
    ModeItem {
        id: "gaming",
        label: "Gaming",
        description: "Prioritizes frame pacing and interactive response during games.",
    },
    ModeItem {
        id: "lowlatency",
        label: "Low latency",
        description: "Favours quick wakeups for audio, capture, and latency-sensitive work.",
    },
    ModeItem {
        id: "powersave",
        label: "Power save",
        description: "Reduces power draw, heat, and fan noise when raw throughput matters less.",
    },
    ModeItem {
        id: "server",
        label: "Server",
        description: "Favours throughput and steady parallel work over desktop snappiness.",
    },
];

pub struct PerformanceService {
    store: JsonStore,
    runner: Arc<dyn CommandRunner>,
}

impl PerformanceService {
    pub fn new(config_home: PathBuf, runner: Arc<dyn CommandRunner>) -> Self {
        Self {
            store: JsonStore::new(config_home.join("AstreaOS/system/performance.json")),
            runner,
        }
    }

    /// Read configuration and probe the host. This method never writes or applies settings.
    pub fn snapshot(&self) -> Result<Value> {
        let config = normalized_config(self.store.read()?)?;
        let status = self.read_status();
        Ok(json!({ "config": config, "status": status }))
    }

    pub fn set_preference(&self, key: &str, value: Value) -> Result<Value> {
        validate_preference(key, &value)?;
        self.store.update(|doc| {
            let _ = normalized_config(doc.clone())?;
            doc.insert(key.to_string(), value);
            Ok(())
        })?;
        self.snapshot()
    }

    pub fn apply_profile(&self, profile: &str) -> Result<Value> {
        validate_profile(profile)?;
        let _ = normalized_config(self.store.read()?)?;
        require_success(
            self.runner.as_ref(),
            "powerprofilesctl",
            &["set".into(), profile.into()],
        )?;
        self.store
            .update(|doc| {
                let _ = normalized_config(doc.clone())?;
                doc.insert("profile".into(), Value::String(profile.into()));
                Ok(())
            })
            .map_err(|e| format!("power profile applied, but saving its preference failed: {e}"))?;
        self.snapshot()
    }

    pub fn apply_scheduler(&self, scheduler: &str, mode: &str) -> Result<Value> {
        validate_scheduler(scheduler)?;
        validate_mode(mode)?;
        let _ = normalized_config(self.store.read()?)?;
        if !self.runner.available("scxctl") {
            return Err("scxctl is not installed or is not executable".into());
        }
        let status = self.read_scx_status();
        if !status["supported"].as_array().is_none_or(Vec::is_empty) {
            let supported = status["supported"].as_array().expect("checked array");
            if !supported.iter().any(|v| v.as_str() == Some(scheduler)) {
                return Err(format!(
                    "scheduler {scheduler} is not supported on this system"
                ));
            }
        }
        require_success(
            self.runner.as_ref(),
            "scxctl",
            &[
                "switch".into(),
                "--sched".into(),
                scheduler.into(),
                "--mode".into(),
                mode.into(),
            ],
        )?;
        self.store
            .update(|doc| {
                let _ = normalized_config(doc.clone())?;
                doc.insert("scx_scheduler".into(), Value::String(scheduler.into()));
                doc.insert("scx_mode".into(), Value::String(mode.into()));
                Ok(())
            })
            .map_err(|e| format!("scheduler applied, but saving its preference failed: {e}"))?;
        self.snapshot()
    }

    fn read_status(&self) -> Value {
        let has_ppd = self.runner.available("powerprofilesctl");
        let has_gamemode = self.runner.available("gamemoded");
        let mut errors = Vec::new();
        let mut profile = "unknown".to_string();
        if has_ppd {
            match self.runner.run("powerprofilesctl", &["get".into()]) {
                Ok(out) if out.code == 0 && !out.stdout.trim().is_empty() => {
                    profile = out.stdout.trim().to_string()
                }
                Ok(out) => errors.push(command_error("powerprofilesctl get", &out)),
                Err(e) => errors.push(e),
            }
        }
        let mut gamemode_active = false;
        if has_gamemode {
            match self.runner.run("gamemoded", &["-s".into()]) {
                Ok(out) => gamemode_active = out.code == 0,
                Err(e) => errors.push(e),
            }
        }
        let mut scx = self.read_scx_status();
        let scx_error = scx["error"].as_str().unwrap_or_default().to_string();
        if !scx_error.is_empty() {
            errors.push(scx_error);
        }
        let error = errors.join("; ");
        scx["error"] = Value::String(error.clone());
        json!({
            "powerprofilesctl": has_ppd,
            "gamemode": has_gamemode,
            "gamemode_active": gamemode_active,
            "profile": profile,
            "error": error,
            "scx": scx,
        })
    }

    fn read_scx_status(&self) -> Value {
        let has_scxctl = self.runner.available("scxctl");
        let mut supported = SCHEDULERS
            .iter()
            .filter(|item| self.runner.available(&format!("scx_{}", item.id)))
            .map(|item| item.id.to_string())
            .collect::<Vec<_>>();
        let mut current = empty_current(String::new());
        let mut errors = Vec::new();
        if has_scxctl {
            match self.runner.run("scxctl", &["get".into()]) {
                Ok(out) if out.code == 0 => current = parse_scxctl_get(&out.stdout),
                Ok(out) => errors.push(command_error("scxctl get", &out)),
                Err(e) => errors.push(e),
            }
            match self.runner.run("scxctl", &["list".into()]) {
                Ok(out) if out.code == 0 => {
                    let listed = parse_scxctl_list(&out.stdout);
                    if !listed.is_empty() {
                        supported = listed;
                    }
                }
                Ok(out) => errors.push(command_error("scxctl list", &out)),
                Err(e) => errors.push(e),
            }
        }
        let mut service = "unknown".to_string();
        if self.runner.available("systemctl") {
            match self.runner.run(
                "systemctl",
                &["is-active".into(), "scx_loader.service".into()],
            ) {
                Ok(out) => {
                    service = first_nonempty(&out.stdout, &out.stderr)
                        .unwrap_or("unknown")
                        .to_string()
                }
                Err(e) => errors.push(e),
            }
        }
        json!({
            "scxctl": has_scxctl,
            "service": service,
            "current": current,
            "supported": supported,
            "catalog": SCHEDULERS.iter().map(|item| json!({"id":item.id,"label":item.label,"best_for":item.best_for,"description":item.description})).collect::<Vec<_>>(),
            "modes": MODES.iter().map(|item| json!({"id":item.id,"label":item.label,"description":item.description})).collect::<Vec<_>>(),
            "error": errors.join("; "),
        })
    }
}

fn normalized_config(mut doc: Map<String, Value>) -> Result<Map<String, Value>> {
    for (key, default) in default_config() {
        let value = doc.get(key).unwrap_or(&default);
        validate_preference(key, value)
            .map_err(|e| format!("invalid performance preference: {e}"))?;
        doc.entry(key).or_insert(default);
    }
    // The legacy helper always reports auto_apply enabled, while preserving all
    // unrelated stored keys. Keep that read-time compatibility without writing.
    doc.insert("auto_apply".into(), json!(true));
    Ok(doc)
}

fn validate_preference(key: &str, value: &Value) -> Result<()> {
    match key {
        "profile" => validate_profile(value.as_str().ok_or("profile must be a string")?),
        "prefer_gamemode" => {
            if value.is_boolean() {
                Ok(())
            } else {
                Err("prefer_gamemode must be a boolean".into())
            }
        }
        "scx_scheduler" => {
            validate_scheduler(value.as_str().ok_or("scx_scheduler must be a string")?)
        }
        "scx_mode" => validate_mode(value.as_str().ok_or("scx_mode must be a string")?),
        "auto_apply"
        | "launch_boost"
        | "reduce_effects"
        | "limit_background_tasks"
        | "show_status_badges" => {
            if value.is_boolean() {
                Ok(())
            } else {
                Err(format!("{key} must be a boolean"))
            }
        }
        _ => Err(format!("unsupported preference key: {key}")),
    }
}

fn validate_profile(value: &str) -> Result<()> {
    if PROFILE_IDS.contains(&value) {
        Ok(())
    } else {
        Err(format!("unsupported power profile: {value}"))
    }
}
fn validate_scheduler(value: &str) -> Result<()> {
    if SCHEDULERS.iter().any(|item| item.id == value) {
        Ok(())
    } else {
        Err(format!("unsupported scheduler: {value}"))
    }
}
fn validate_mode(value: &str) -> Result<()> {
    if MODES.iter().any(|item| item.id == value) {
        Ok(())
    } else {
        Err(format!("unsupported scheduler mode: {value}"))
    }
}

fn require_success(runner: &dyn CommandRunner, program: &str, args: &[String]) -> Result<()> {
    if !runner.available(program) {
        return Err(format!("{program} is not installed or is not executable"));
    }
    let output = runner.run(program, args)?;
    if output.code == 0 {
        Ok(())
    } else {
        Err(command_error(program, &output))
    }
}

fn command_error(program: &str, output: &crate::command::CommandOutput) -> String {
    let detail = first_nonempty(&output.stderr, &output.stdout).unwrap_or("no details");
    format!(
        "{program} failed with exit code {}: {}",
        output.code,
        detail.trim()
    )
}
fn first_nonempty<'a>(first: &'a str, second: &'a str) -> Option<&'a str> {
    [first, second]
        .into_iter()
        .find(|s| !s.trim().is_empty())
        .map(str::trim)
}
fn empty_current(raw: String) -> Value {
    json!({"active":false,"scheduler":"","scheduler_label":"","mode":"","mode_label":"","raw":raw})
}

fn parse_scxctl_get(output: &str) -> Value {
    let text = output.trim();
    let Some(running_at) = text.to_ascii_lowercase().find("running ") else {
        return empty_current(text.to_string());
    };
    let rest = &text[running_at + "running ".len()..];
    let Some(in_at) = rest.to_ascii_lowercase().find(" in ") else {
        return empty_current(text.to_string());
    };
    let scheduler_label = rest[..in_at].trim();
    let after_in = &rest[in_at + 4..];
    let Some(mode_at) = after_in.to_ascii_lowercase().find(" mode") else {
        return empty_current(text.to_string());
    };
    let mode_label = after_in[..mode_at].trim();
    if scheduler_label.is_empty() || mode_label.is_empty() {
        return empty_current(text.to_string());
    }
    let scheduler = scheduler_label.to_ascii_lowercase();
    let mode = mode_label
        .to_ascii_lowercase()
        .chars()
        .filter(|c| c.is_ascii_alphanumeric())
        .collect::<String>();
    json!({"active":true,"scheduler":scheduler,"scheduler_label":scheduler_label,"mode":mode,"mode_label":mode_label,"raw":text})
}

fn parse_scxctl_list(output: &str) -> Vec<String> {
    let text = output.trim();
    if text.is_empty() {
        return Vec::new();
    }
    let raw = text
        .split_once(':')
        .map(|(_, tail)| tail.trim())
        .filter(|s| !s.is_empty())
        .unwrap_or(text);
    let Some(start) = raw.find('[') else {
        return Vec::new();
    };
    let Some(end) = raw.rfind(']') else {
        return Vec::new();
    };
    if end <= start {
        return Vec::new();
    }
    raw[start + 1..end]
        .split(',')
        .filter_map(|part| {
            let item = part.trim().trim_matches(|c| c == '\'' || c == '"').trim();
            if item.is_empty() {
                None
            } else {
                let item = item.to_ascii_lowercase();
                Some(item.strip_prefix("scx_").unwrap_or(&item).to_string())
            }
        })
        .collect()
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::command::CommandOutput;
    use std::{collections::HashMap, sync::Mutex};

    #[derive(Default)]
    struct FakeRunner {
        available: Mutex<Vec<String>>,
        replies: Mutex<HashMap<(String, Vec<String>), CommandOutput>>,
        calls: Mutex<Vec<(String, Vec<String>)>>,
    }
    impl FakeRunner {
        fn available_programs(programs: &[&str]) -> Self {
            Self {
                available: Mutex::new(programs.iter().map(|p| p.to_string()).collect()),
                ..Self::default()
            }
        }
        fn reply(&self, program: &str, args: &[&str], code: i32, stdout: &str, stderr: &str) {
            self.replies.lock().unwrap().insert(
                (program.into(), args.iter().map(|s| s.to_string()).collect()),
                CommandOutput {
                    code,
                    stdout: stdout.into(),
                    stderr: stderr.into(),
                },
            );
        }
        fn calls(&self) -> Vec<(String, Vec<String>)> {
            self.calls.lock().unwrap().clone()
        }
    }
    impl CommandRunner for FakeRunner {
        fn available(&self, program: &str) -> bool {
            self.available.lock().unwrap().iter().any(|p| p == program)
        }
        fn run(&self, program: &str, args: &[String]) -> Result<CommandOutput> {
            let key = (program.to_string(), args.to_vec());
            self.calls.lock().unwrap().push(key.clone());
            self.replies
                .lock()
                .unwrap()
                .get(&key)
                .cloned()
                .ok_or_else(|| format!("no fake reply for {program} {args:?}"))
        }
    }
    fn service(dir: &std::path::Path, runner: Arc<FakeRunner>) -> PerformanceService {
        PerformanceService::new(dir.to_path_buf(), runner)
    }

    #[test]
    fn snapshot_defaults_are_read_only_and_report_unavailable_tools() {
        let dir = tempfile::tempdir().unwrap();
        let runner = Arc::new(FakeRunner::default());
        let snapshot = service(dir.path(), runner.clone()).snapshot().unwrap();
        assert_eq!(snapshot["config"]["profile"], "balanced");
        assert_eq!(snapshot["config"]["scx_scheduler"], "bpfland");
        assert_eq!(snapshot["config"]["auto_apply"], true);
        assert_eq!(snapshot["status"]["powerprofilesctl"], false);
        assert_eq!(snapshot["status"]["gamemode"], false);
        assert!(!dir.path().join("AstreaOS/system/performance.json").exists());
        assert!(runner.calls().is_empty());
    }

    #[test]
    fn snapshot_preserves_unknown_fields_and_does_not_rewrite_file() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("AstreaOS/system/performance.json");
        std::fs::create_dir_all(path.parent().unwrap()).unwrap();
        let original = br#"{"profile":"performance","future_option":{"x":1}}"#;
        std::fs::write(&path, original).unwrap();
        let snapshot = service(dir.path(), Arc::new(FakeRunner::default()))
            .snapshot()
            .unwrap();
        assert_eq!(snapshot["config"]["future_option"]["x"], 1);
        assert_eq!(snapshot["config"]["prefer_gamemode"], true);
        assert_eq!(std::fs::read(path).unwrap(), original);
    }

    #[test]
    fn corrupt_or_mistyped_config_errors_without_repairing_file() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("AstreaOS/system/performance.json");
        std::fs::create_dir_all(path.parent().unwrap()).unwrap();
        let original = br#"{"prefer_gamemode":"yes"}"#;
        std::fs::write(&path, original).unwrap();
        assert!(
            service(dir.path(), Arc::new(FakeRunner::default()))
                .snapshot()
                .is_err()
        );
        assert_eq!(std::fs::read(path).unwrap(), original);
    }

    #[test]
    fn profile_apply_uses_exact_argv_and_does_not_save_on_command_failure() {
        let dir = tempfile::tempdir().unwrap();
        let runner = Arc::new(FakeRunner::available_programs(&["powerprofilesctl"]));
        runner.reply("powerprofilesctl", &["set", "performance"], 3, "", "denied");
        let error = service(dir.path(), runner.clone())
            .apply_profile("performance")
            .unwrap_err();
        assert!(error.contains("denied"));
        assert_eq!(
            runner.calls(),
            vec![(
                "powerprofilesctl".into(),
                vec!["set".into(), "performance".into()]
            )]
        );
        assert!(!dir.path().join("AstreaOS/system/performance.json").exists());
    }

    #[test]
    fn scheduler_apply_uses_exact_argv_and_persists_only_after_success() {
        let dir = tempfile::tempdir().unwrap();
        let runner = Arc::new(FakeRunner::available_programs(&["scxctl"]));
        runner.reply(
            "scxctl",
            &["get"],
            0,
            "running Bpfland in Gaming mode\n",
            "",
        );
        runner.reply(
            "scxctl",
            &["list"],
            0,
            "Available schedulers: ['bpfland', 'lavd']",
            "",
        );
        runner.reply(
            "scxctl",
            &["switch", "--sched", "lavd", "--mode", "lowlatency"],
            0,
            "",
            "",
        );
        runner.reply(
            "scxctl",
            &["get"],
            0,
            "running LAVD in Low latency mode\n",
            "",
        );
        runner.reply(
            "scxctl",
            &["list"],
            0,
            "Available schedulers: ['bpfland', 'lavd']",
            "",
        );
        let snapshot = service(dir.path(), runner.clone())
            .apply_scheduler("lavd", "lowlatency")
            .unwrap();
        assert_eq!(snapshot["config"]["scx_scheduler"], "lavd");
        assert_eq!(snapshot["config"]["scx_mode"], "lowlatency");
        assert!(runner.calls().contains(&(
            "scxctl".into(),
            vec![
                "switch".into(),
                "--sched".into(),
                "lavd".into(),
                "--mode".into(),
                "lowlatency".into()
            ]
        )));
    }

    #[test]
    fn parser_handles_legacy_scxctl_samples() {
        let current = parse_scxctl_get("Running Bpfland in Low latency mode (v1)\n");
        assert_eq!(current["scheduler"], "bpfland");
        assert_eq!(current["mode"], "lowlatency");
        assert!(current["active"].as_bool().unwrap());
        assert_eq!(
            parse_scxctl_list("Available schedulers: ['bpfland', 'LAVD']"),
            vec!["bpfland", "lavd"]
        );
        assert_eq!(
            parse_scxctl_list("Available schedulers: ['scx_bpfland']"),
            vec!["bpfland"]
        );
        assert_eq!(parse_scxctl_list(""), Vec::<String>::new());
    }
}
