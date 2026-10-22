use crate::{Result, launch, store::JsonStore};
use serde_json::{Map, Value, json};
use std::path::{Path, PathBuf};

fn proton_defaults() -> Vec<(&'static str, Value)> {
    vec![
        ("config_version", json!(2)),
        ("preset", json!("recommended")),
        ("gamemode", json!(true)),
        ("mangohud", json!(false)),
        ("gamescope", json!(false)),
        ("use_gamescope_profile", json!(true)),
        ("gamescope_width", json!(1920)),
        ("gamescope_height", json!(1080)),
        ("gamescope_refresh", json!(60)),
        ("gamescope_fullscreen", json!(true)),
        ("gamescope_immediate_flips", json!(false)),
        ("gamescope_hide_cursor", json!(false)),
        ("gamescope_force_grab_cursor", json!(false)),
        ("gamescope_adaptive_sync", json!(false)),
        ("gamescope_extra_args", json!("")),
        ("enable_nvapi", json!(false)),
        ("hide_nvidia_gpu", json!(false)),
        ("sync_mode", json!("default")),
        ("enable_esync", json!(true)),
        ("enable_fsync", json!(true)),
        ("dxvk_async", json!(false)),
        ("dxvk_hdr", json!(false)),
        ("vkd3d_dxr", json!(false)),
        ("use_wined3d", json!(false)),
        ("fsr", json!(false)),
        ("fsr_strength", json!(2)),
        ("custom_env", json!("")),
        ("custom_prefix", json!("")),
    ]
}
fn compat_defaults() -> Vec<(&'static str, Value)> {
    vec![
        ("config_version", json!(1)),
        ("runner", json!("proton")),
        ("use_proton_profile", json!(true)),
        ("gamemode", json!(true)),
        ("mangohud", json!(false)),
        ("gamescope", json!(false)),
        ("extra_env", json!("")),
        ("extra_prefix", json!("")),
    ]
}
fn gamescope_defaults() -> Vec<(&'static str, Value)> {
    vec![
        ("follow_monitor", json!(true)),
        ("width", json!(1920)),
        ("height", json!(1080)),
        ("refresh", json!(165)),
        ("fullscreen", json!(true)),
        ("steam_integration", json!(true)),
        ("backend_drm", json!(true)),
        ("immediate_flips", json!(true)),
        ("hide_cursor", json!(true)),
        ("fancy_scaling", json!(true)),
        ("color_managed", json!(true)),
        ("virtual_white", json!(true)),
        ("multiple_xwaylands", json!(true)),
        ("hdr", json!(false)),
        ("output_connector", json!("*,eDP-1")),
        ("extra_args", json!("")),
    ]
}

pub struct GamingService {
    root: PathBuf,
}

impl GamingService {
    pub fn new(config_home: PathBuf) -> Self {
        Self { root: config_home }
    }
    fn store(&self, name: &str) -> JsonStore {
        JsonStore::new(self.root.join("AstreaOS/gaming").join(name))
    }
    fn proton(&self) -> Result<Map<String, Value>> {
        normalize_proton(self.store("proton.json").read()?)
    }
    fn compatibility(&self) -> Result<Map<String, Value>> {
        normalize_compatibility(self.store("compatibility.json").read()?)
    }
    fn gamescope(&self) -> Result<Map<String, Value>> {
        normalize_gamescope(self.store("gamescope.json").read()?)
    }

    pub fn snapshot(&self) -> Result<Value> {
        let proton = self.proton()?;
        let compatibility = self.compatibility()?;
        let gamescope = self.gamescope()?;
        let root = self.root.clone();
        let proton_status = launch::proton_status(&proton, &gamescope, &root)?;
        Ok(
            json!({"proton":proton,"compatibility":compatibility,"gamescope":gamescope,"status":proton_status}),
        )
    }

    pub fn set_proton(&self, key: &str, value: Value) -> Result<Value> {
        let mut probe = normalize_proton(Map::new())?;
        apply_proton(&mut probe, key, value.clone())?;
        self.store("proton.json").update(|doc| {
            let mut current = normalize_proton(doc.clone())?;
            apply_proton(&mut current, key, value)?;
            *doc = current;
            Ok(())
        })?;
        self.snapshot()
    }

    pub fn set_compatibility(&self, key: &str, value: Value) -> Result<Value> {
        let mut probe = normalize_compatibility(Map::new())?;
        apply_compatibility(&mut probe, key, value.clone())?;
        self.store("compatibility.json").update(|doc| {
            let mut current = normalize_compatibility(doc.clone())?;
            apply_compatibility(&mut current, key, value)?;
            *doc = current;
            Ok(())
        })?;
        self.snapshot()
    }

    pub fn config_root(&self) -> &Path {
        &self.root
    }
}

fn defaults(fields: Vec<(&str, Value)>, raw: Map<String, Value>) -> Map<String, Value> {
    let mut out: Map<String, Value> = fields
        .iter()
        .map(|(k, v)| ((*k).to_string(), v.clone()))
        .collect();
    out.extend(raw);
    out
}
fn bool_field(map: &Map<String, Value>, key: &str) -> Result<bool> {
    map.get(key)
        .and_then(Value::as_bool)
        .ok_or_else(|| format!("{key} must be a boolean"))
}
fn int_range(map: &Map<String, Value>, key: &str, min: i64, max: i64) -> Result<()> {
    let value = map
        .get(key)
        .and_then(Value::as_i64)
        .ok_or_else(|| format!("{key} must be an integer"))?;
    if !(min..=max).contains(&value) {
        return Err(format!("{key} must be between {min} and {max}"));
    }
    Ok(())
}
fn text_field<'a>(map: &'a Map<String, Value>, key: &str) -> Result<&'a str> {
    map.get(key)
        .and_then(Value::as_str)
        .ok_or_else(|| format!("{key} must be a string"))
}
fn normalize_proton(raw: Map<String, Value>) -> Result<Map<String, Value>> {
    let mut m = defaults(proton_defaults(), raw);
    let version = m
        .get("config_version")
        .and_then(Value::as_i64)
        .ok_or("config_version must be an integer")?;
    if version > 2 {
        return Err(format!("unsupported Proton config_version: {version}"));
    }
    if version < 2 {
        let esync = m
            .get("enable_esync")
            .and_then(Value::as_bool)
            .unwrap_or(true);
        let fsync = m
            .get("enable_fsync")
            .and_then(Value::as_bool)
            .unwrap_or(true);
        m.insert("preset".into(), json!("recommended"));
        m.insert("enable_nvapi".into(), json!(false));
        m.insert(
            "sync_mode".into(),
            json!(match (esync, fsync) {
                (false, false) => "disable-both",
                (false, true) => "disable-esync",
                (true, false) => "disable-fsync",
                _ => "default",
            }),
        );
    }
    m.insert("config_version".into(), json!(2));
    let preset = text_field(&m, "preset")?;
    if !["recommended", "nvidia", "hdr", "diagnostic", "custom"].contains(&preset) {
        m.insert("preset".into(), json!("custom"));
    }
    let sync = text_field(&m, "sync_mode")?.to_string();
    if !["default", "disable-esync", "disable-fsync", "disable-both"].contains(&sync.as_str()) {
        return Err("sync_mode is invalid".into());
    }
    for key in [
        "gamemode",
        "mangohud",
        "gamescope",
        "use_gamescope_profile",
        "gamescope_fullscreen",
        "gamescope_immediate_flips",
        "gamescope_hide_cursor",
        "gamescope_force_grab_cursor",
        "gamescope_adaptive_sync",
        "enable_nvapi",
        "hide_nvidia_gpu",
        "dxvk_async",
        "dxvk_hdr",
        "vkd3d_dxr",
        "use_wined3d",
        "fsr",
    ] {
        bool_field(&m, key)?;
    }
    m.insert(
        "enable_esync".into(),
        json!(!["disable-esync", "disable-both"].contains(&sync.as_str())),
    );
    m.insert(
        "enable_fsync".into(),
        json!(!["disable-fsync", "disable-both"].contains(&sync.as_str())),
    );
    int_range(&m, "fsr_strength", 0, 5)?;
    int_range(&m, "gamescope_width", 640, 10000)?;
    int_range(&m, "gamescope_height", 360, 10000)?;
    int_range(&m, "gamescope_refresh", 30, 1000)?;
    for key in ["gamescope_extra_args", "custom_env", "custom_prefix"] {
        text_field(&m, key)?;
    }
    crate::launch::env_words(text_field(&m, "custom_env")?, "custom_env")?;
    crate::launch::parse_words(text_field(&m, "custom_prefix")?, "custom_prefix")?;
    crate::launch::parse_words(
        text_field(&m, "gamescope_extra_args")?,
        "gamescope_extra_args",
    )?;
    Ok(m)
}
fn normalize_compatibility(raw: Map<String, Value>) -> Result<Map<String, Value>> {
    let mut m = defaults(compat_defaults(), raw);
    let version = m
        .get("config_version")
        .and_then(Value::as_i64)
        .ok_or("config_version must be an integer")?;
    if version > 1 {
        return Err(format!(
            "unsupported compatibility config_version: {version}"
        ));
    }
    m.insert("config_version".into(), json!(1));
    if !["proton", "wine", "auto"].contains(&text_field(&m, "runner")?) {
        m.insert("runner".into(), json!("proton"));
    }
    for key in ["use_proton_profile", "gamemode", "mangohud", "gamescope"] {
        bool_field(&m, key)?;
    }
    for key in ["extra_env", "extra_prefix"] {
        text_field(&m, key)?;
    }
    crate::launch::env_words(text_field(&m, "extra_env")?, "extra_env")?;
    crate::launch::parse_words(text_field(&m, "extra_prefix")?, "extra_prefix")?;
    Ok(m)
}
fn normalize_gamescope(raw: Map<String, Value>) -> Result<Map<String, Value>> {
    let m = defaults(gamescope_defaults(), raw);
    for key in [
        "follow_monitor",
        "fullscreen",
        "steam_integration",
        "backend_drm",
        "immediate_flips",
        "hide_cursor",
        "fancy_scaling",
        "color_managed",
        "virtual_white",
        "multiple_xwaylands",
        "hdr",
    ] {
        bool_field(&m, key)?;
    }
    int_range(&m, "width", 640, 10000)?;
    int_range(&m, "height", 360, 10000)?;
    int_range(&m, "refresh", 30, 1000)?;
    for key in ["output_connector", "extra_args"] {
        text_field(&m, key)?;
    }
    crate::launch::parse_words(text_field(&m, "extra_args")?, "gamescope extra_args")?;
    Ok(m)
}

fn apply_proton(m: &mut Map<String, Value>, key: &str, value: Value) -> Result<()> {
    if key == "preset" {
        let preset = value.as_str().ok_or("preset must be a string")?;
        if !["recommended", "nvidia", "hdr", "diagnostic"].contains(&preset) {
            return Err("preset is invalid".into());
        }
        let fields: &[(&str, Value)] = match preset {
            "nvidia" => &[
                ("gamemode", json!(true)),
                ("enable_nvapi", json!(true)),
                ("hide_nvidia_gpu", json!(false)),
                ("sync_mode", json!("default")),
                ("vkd3d_dxr", json!(false)),
            ],
            "hdr" => &[
                ("gamemode", json!(true)),
                ("dxvk_hdr", json!(true)),
                ("sync_mode", json!("default")),
            ],
            "diagnostic" => &[
                ("gamemode", json!(true)),
                ("mangohud", json!(true)),
                ("sync_mode", json!("default")),
            ],
            _ => &[
                ("gamemode", json!(true)),
                ("mangohud", json!(false)),
                ("enable_nvapi", json!(false)),
                ("hide_nvidia_gpu", json!(false)),
                ("sync_mode", json!("default")),
                ("dxvk_hdr", json!(false)),
                ("vkd3d_dxr", json!(false)),
                ("use_wined3d", json!(false)),
                ("fsr", json!(false)),
            ],
        };
        for (k, v) in fields {
            m.insert((*k).into(), v.clone());
        }
        m.insert("preset".into(), json!(preset));
        m.insert("custom_env".into(), json!(""));
        m.insert("custom_prefix".into(), json!(""));
        let sync = m["sync_mode"].as_str().unwrap().to_string();
        m.insert(
            "enable_esync".into(),
            json!(!["disable-esync", "disable-both"].contains(&sync.as_str())),
        );
        m.insert(
            "enable_fsync".into(),
            json!(!["disable-fsync", "disable-both"].contains(&sync.as_str())),
        );
        return Ok(());
    }
    if key == "config_version" || key == "enable_esync" || key == "enable_fsync" {
        return Err(format!("{key} is managed by the sync mode"));
    }
    let allowed_bool = [
        "gamemode",
        "mangohud",
        "gamescope",
        "use_gamescope_profile",
        "gamescope_fullscreen",
        "gamescope_immediate_flips",
        "gamescope_hide_cursor",
        "gamescope_force_grab_cursor",
        "gamescope_adaptive_sync",
        "enable_nvapi",
        "hide_nvidia_gpu",
        "dxvk_async",
        "dxvk_hdr",
        "vkd3d_dxr",
        "use_wined3d",
        "fsr",
    ];
    let allowed_string = [
        "sync_mode",
        "gamescope_extra_args",
        "custom_env",
        "custom_prefix",
    ];
    if allowed_bool.contains(&key) {
        if !value.is_boolean() {
            return Err(format!("{key} must be a boolean"));
        }
    } else if allowed_string.contains(&key) {
        if !value.is_string() {
            return Err(format!("{key} must be a string"));
        }
        if key == "sync_mode"
            && !["default", "disable-esync", "disable-fsync", "disable-both"]
                .contains(&value.as_str().unwrap())
        {
            return Err("sync_mode is invalid".into());
        }
        match key {
            "custom_env" => {
                crate::launch::env_words(value.as_str().unwrap(), key)?;
            }
            "custom_prefix" | "gamescope_extra_args" => {
                crate::launch::parse_words(value.as_str().unwrap(), key)?;
            }
            _ => {}
        }
    } else if key == "fsr_strength"
        || key == "gamescope_width"
        || key == "gamescope_height"
        || key == "gamescope_refresh"
    {
        if !value.is_i64() {
            return Err(format!("{key} must be an integer"));
        }
        let (low, high) = match key {
            "fsr_strength" => (0, 5),
            "gamescope_width" => (640, 10000),
            "gamescope_height" => (360, 10000),
            _ => (30, 1000),
        };
        if !(low..=high).contains(&value.as_i64().unwrap()) {
            return Err(format!("{key} must be between {low} and {high}"));
        }
    } else {
        return Err(format!("unknown Proton setting: {key}"));
    }
    m.insert(key.into(), value);
    if [
        "gamemode",
        "mangohud",
        "enable_nvapi",
        "hide_nvidia_gpu",
        "dxvk_hdr",
        "vkd3d_dxr",
        "use_wined3d",
        "fsr",
        "fsr_strength",
    ]
    .contains(&key)
        || key.starts_with("gamescope_")
        || key == "sync_mode"
    {
        m.insert("preset".into(), json!("custom"));
    }
    if key == "sync_mode" {
        let sync = m[key].as_str().unwrap().to_string();
        m.insert(
            "enable_esync".into(),
            json!(!["disable-esync", "disable-both"].contains(&sync.as_str())),
        );
        m.insert(
            "enable_fsync".into(),
            json!(!["disable-fsync", "disable-both"].contains(&sync.as_str())),
        );
    }
    Ok(())
}
fn apply_compatibility(m: &mut Map<String, Value>, key: &str, value: Value) -> Result<()> {
    match key {
        "runner" => {
            if !["proton", "wine", "auto"]
                .contains(&value.as_str().ok_or("runner must be a string")?)
            {
                return Err("runner must be proton, wine, or auto".into());
            }
        }
        "use_proton_profile" | "gamemode" | "mangohud" | "gamescope" => {
            if !value.is_boolean() {
                return Err(format!("{key} must be a boolean"));
            }
        }
        "extra_env" | "extra_prefix" => {
            if !value.is_string() {
                return Err(format!("{key} must be a string"));
            }
            if key == "extra_env" {
                crate::launch::env_words(value.as_str().unwrap(), key)?;
            } else {
                crate::launch::parse_words(value.as_str().unwrap(), key)?;
            }
        }
        _ => return Err(format!("unknown compatibility setting: {key}")),
    }
    m.insert(key.into(), value);
    Ok(())
}
