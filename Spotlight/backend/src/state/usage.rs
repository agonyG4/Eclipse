use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;

const DEFAULT_USAGE_PATH: &str = "Astrea/spotlight-usage.json";

fn xdg_state_home() -> PathBuf {
    std::env::var("XDG_STATE_HOME")
        .map(PathBuf::from)
        .unwrap_or_else(|_| {
            let home = std::env::var("HOME").unwrap_or_else(|_| "/tmp".to_string());
            PathBuf::from(home).join(".local/state")
        })
}

fn usage_path(astrea_root: &str) -> PathBuf {
    let explicit = PathBuf::from(astrea_root)
        .join("state")
        .join(DEFAULT_USAGE_PATH);
    if explicit.exists() {
        return explicit;
    }
    xdg_state_home().join(DEFAULT_USAGE_PATH)
}

pub fn load_usage(astrea_root: &str) -> HashMap<String, i32> {
    let path = usage_path(astrea_root);
    match fs::read_to_string(&path) {
        Ok(text) => serde_json::from_str(&text).unwrap_or_default(),
        Err(_) => HashMap::new(),
    }
}

pub fn save_usage(astrea_root: &str, usage: &HashMap<String, i32>) -> Result<(), String> {
    let path = usage_path(astrea_root);
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|e| format!("create usage dir: {e}"))?;
    }
    let json = serde_json::to_string(usage).map_err(|e| format!("serialize usage: {e}"))?;
    let tmp_path = path.with_extension("tmp");
    fs::write(&tmp_path, &json).map_err(|e| format!("write usage tmp: {e}"))?;
    fs::rename(&tmp_path, &path).map_err(|e| format!("rename usage: {e}"))?;
    Ok(())
}

pub fn record_launch(astrea_root: &str, desktop_id: &str) -> Result<(), String> {
    let mut usage = load_usage(astrea_root);
    let count = usage.entry(desktop_id.to_string()).or_insert(0);
    *count += 1;
    save_usage(astrea_root, &usage)
}
