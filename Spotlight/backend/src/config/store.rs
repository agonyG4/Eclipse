use serde::{Deserialize, Serialize};
use std::fs;
use std::path::PathBuf;

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SpotlightConfig {
    #[serde(default = "default_true")]
    pub weather: bool,
}

fn default_true() -> bool {
    true
}

impl Default for SpotlightConfig {
    fn default() -> Self {
        Self { weather: true }
    }
}

pub fn config_path() -> PathBuf {
    let config_home = std::env::var("XDG_CONFIG_HOME")
        .map(PathBuf::from)
        .unwrap_or_else(|_| {
            let home = std::env::var("HOME").unwrap_or_else(|_| "/tmp".to_string());
            PathBuf::from(home).join(".config")
        });
    config_home.join("AstreaOS/spotlight.json")
}

pub fn load_config() -> SpotlightConfig {
    let path = config_path();
    match fs::read_to_string(&path) {
        Ok(text) => serde_json::from_str(&text).unwrap_or_default(),
        Err(_) => SpotlightConfig::default(),
    }
}

pub fn write_default_config() -> Result<(), String> {
    let path = config_path();
    if path.exists() {
        return Ok(());
    }
    if let Some(parent) = path.parent() {
        fs::create_dir_all(parent).map_err(|e| format!("create config dir: {e}"))?;
    }
    let default = SpotlightConfig::default();
    let json =
        serde_json::to_string_pretty(&default).map_err(|e| format!("serialize config: {e}"))?;
    fs::write(&path, &json).map_err(|e| format!("write config: {e}"))?;
    Ok(())
}
