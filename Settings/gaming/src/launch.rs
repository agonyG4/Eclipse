use crate::store::JsonStore;
use crate::{Result, config::GamingService, store::config_home};
use serde_json::{Map, Value, json};
use std::{
    collections::BTreeMap,
    env,
    ffi::OsString,
    fs,
    os::unix::process::CommandExt,
    path::{Path, PathBuf},
    process::Command,
};

#[derive(Clone, Debug)]
pub struct LaunchPlan {
    pub program: OsString,
    pub args: Vec<OsString>,
    pub env: BTreeMap<OsString, OsString>,
}

pub fn parse_words(value: &str, field: &str) -> Result<Vec<String>> {
    if value.contains('\0') {
        return Err(format!("{field} must not contain NUL bytes"));
    }
    shlex::split(value).ok_or_else(|| format!("{field} has unmatched shell quotes"))
}
pub fn env_words(value: &str, field: &str) -> Result<BTreeMap<OsString, OsString>> {
    let mut env = BTreeMap::new();
    for word in parse_words(value, field)? {
        let (key, val) = word
            .split_once('=')
            .ok_or_else(|| format!("{field} entries must use NAME=value"))?;
        if key.is_empty()
            || !key
                .bytes()
                .enumerate()
                .all(|(i, b)| b == b'_' || b.is_ascii_alphabetic() || (i > 0 && b.is_ascii_digit()))
        {
            return Err(format!(
                "{field} contains an invalid environment name: {key}"
            ));
        }
        env.insert(key.into(), val.into());
    }
    Ok(env)
}
fn find_executable(name: &str) -> Option<PathBuf> {
    let paths = env::var_os("PATH")?;
    env::split_paths(&paths)
        .map(|p| p.join(name))
        .find(|candidate| candidate.is_file() && is_executable(candidate))
}
fn is_executable(path: &Path) -> bool {
    use std::os::unix::fs::PermissionsExt;
    fs::metadata(path)
        .map(|m| m.permissions().mode() & 0o111 != 0)
        .unwrap_or(false)
}
fn wrapper(name: &str) -> Option<OsString> {
    find_executable(name).map(|p| p.into_os_string())
}

pub fn proton_environment(cfg: &Map<String, Value>) -> Result<BTreeMap<OsString, OsString>> {
    let mut out = BTreeMap::new();
    if cfg.get("enable_nvapi").and_then(Value::as_bool) == Some(true) {
        out.insert("PROTON_FORCE_NVAPI".into(), "1".into());
        out.insert("PROTON_HIDE_NVIDIA_GPU".into(), "0".into());
    } else if cfg.get("hide_nvidia_gpu").and_then(Value::as_bool) == Some(true) {
        out.insert("PROTON_HIDE_NVIDIA_GPU".into(), "1".into());
    }
    let sync = cfg
        .get("sync_mode")
        .and_then(Value::as_str)
        .unwrap_or("default");
    if ["disable-esync", "disable-both"].contains(&sync) {
        out.insert("PROTON_NO_ESYNC".into(), "1".into());
    }
    if ["disable-fsync", "disable-both"].contains(&sync) {
        out.insert("PROTON_NO_FSYNC".into(), "1".into());
    }
    for (key, var) in [
        ("use_wined3d", "PROTON_USE_WINED3D"),
        ("dxvk_async", "DXVK_ASYNC"),
        ("dxvk_hdr", "DXVK_HDR"),
        ("vkd3d_dxr", "VKD3D_CONFIG"),
        ("fsr", "WINE_FULLSCREEN_FSR"),
    ] {
        if cfg.get(key).and_then(Value::as_bool) == Some(true) {
            out.insert(
                var.into(),
                if key == "vkd3d_dxr" {
                    "dxr".into()
                } else {
                    "1".into()
                },
            );
        }
    }
    if cfg.get("fsr").and_then(Value::as_bool) == Some(true) {
        out.insert(
            "WINE_FULLSCREEN_FSR_STRENGTH".into(),
            cfg.get("fsr_strength")
                .and_then(Value::as_i64)
                .unwrap_or(2)
                .to_string()
                .into(),
        );
    }
    out.extend(env_words(
        cfg.get("custom_env").and_then(Value::as_str).unwrap_or(""),
        "custom_env",
    )?);
    Ok(out)
}

pub fn gamescope_argv(
    cfg: &Map<String, Value>,
    mangoapp: bool,
    executable: OsString,
) -> Result<Vec<OsString>> {
    let mut args = vec![executable];
    if cfg.get("fullscreen").and_then(Value::as_bool) == Some(true) {
        args.push("-f".into());
    }
    for (flag, key, default) in [
        ("-W", "width", 1920),
        ("-H", "height", 1080),
        ("-r", "refresh", 60),
    ] {
        args.push(flag.into());
        args.push(
            cfg.get(key)
                .and_then(Value::as_i64)
                .unwrap_or(default)
                .to_string()
                .into(),
        );
    }
    if cfg.get("immediate_flips").and_then(Value::as_bool) == Some(true) {
        args.push("--immediate-flips".into());
    }
    if cfg.get("hide_cursor").and_then(Value::as_bool) == Some(true) {
        args.extend(["--hide-cursor-delay".into(), "-1".into()]);
    }
    if mangoapp {
        args.push("--mangoapp".into());
    }
    args.extend(
        parse_words(
            cfg.get("extra_args").and_then(Value::as_str).unwrap_or(""),
            "gamescope extra_args",
        )?
        .into_iter()
        .map(Into::into),
    );
    args.push("--".into());
    Ok(args)
}

pub fn proton_prefix(
    cfg: &Map<String, Value>,
    gamescope: &Map<String, Value>,
) -> Result<Vec<OsString>> {
    proton_prefix_with_preference(cfg, gamescope, true)
}
pub fn proton_prefix_with_preference(
    cfg: &Map<String, Value>,
    gamescope: &Map<String, Value>,
    prefer_gamemode: bool,
) -> Result<Vec<OsString>> {
    proton_prefix_with_tools(
        cfg,
        gamescope,
        prefer_gamemode,
        find_executable("gamemoderun").map(Into::into),
        find_executable("gamescope").map(Into::into),
        find_executable("mangohud").map(Into::into),
    )
}
pub fn proton_prefix_with_tools(
    cfg: &Map<String, Value>,
    gamescope: &Map<String, Value>,
    prefer_gamemode: bool,
    gamemoderun: Option<OsString>,
    gamescope_bin: Option<OsString>,
    mangohud_bin: Option<OsString>,
) -> Result<Vec<OsString>> {
    let mut prefix = Vec::new();
    if prefer_gamemode
        && cfg.get("gamemode").and_then(Value::as_bool) == Some(true)
        && let Some(p) = gamemoderun
    {
        prefix.push(p);
    }
    let mut mango = cfg.get("mangohud").and_then(Value::as_bool) == Some(true);
    if cfg.get("gamescope").and_then(Value::as_bool) == Some(true)
        && let Some(gs) = gamescope_bin
    {
        prefix.extend(gamescope_argv(
            &proton_gamescope_config(cfg, gamescope)?,
            mango,
            gs,
        )?);
        mango = false;
    }
    if mango && let Some(p) = mangohud_bin {
        prefix.push(p);
    }
    prefix.extend(
        parse_words(
            cfg.get("custom_prefix")
                .and_then(Value::as_str)
                .unwrap_or(""),
            "custom_prefix",
        )?
        .into_iter()
        .map(Into::into),
    );
    Ok(prefix)
}

pub fn proton_gamescope_config(
    proton: &Map<String, Value>,
    shared: &Map<String, Value>,
) -> Result<Map<String, Value>> {
    if proton
        .get("use_gamescope_profile")
        .and_then(Value::as_bool)
        .unwrap_or(true)
    {
        return Ok(shared.clone());
    }
    let mut extra = Vec::new();
    if proton
        .get("gamescope_force_grab_cursor")
        .and_then(Value::as_bool)
        == Some(true)
    {
        extra.push("--force-grab-cursor".to_string());
    }
    if proton
        .get("gamescope_adaptive_sync")
        .and_then(Value::as_bool)
        == Some(true)
    {
        extra.push("--adaptive-sync".to_string());
    }
    extra.extend(parse_words(
        proton
            .get("gamescope_extra_args")
            .and_then(Value::as_str)
            .unwrap_or(""),
        "gamescope_extra_args",
    )?);
    let mut cfg = Map::new();
    cfg.insert("follow_monitor".into(), Value::Bool(false));
    for (target, source, default) in [
        ("width", "gamescope_width", 1920),
        ("height", "gamescope_height", 1080),
        ("refresh", "gamescope_refresh", 60),
    ] {
        cfg.insert(
            target.into(),
            json!(
                proton
                    .get(source)
                    .and_then(Value::as_i64)
                    .unwrap_or(default)
            ),
        );
    }
    for (target, source) in [
        ("fullscreen", "gamescope_fullscreen"),
        ("immediate_flips", "gamescope_immediate_flips"),
        ("hide_cursor", "gamescope_hide_cursor"),
    ] {
        cfg.insert(
            target.into(),
            Value::Bool(proton.get(source).and_then(Value::as_bool).unwrap_or(false)),
        );
    }
    cfg.insert(
        "extra_args".into(),
        Value::String(shlex::try_join(extra.iter().map(String::as_str)).unwrap_or_default()),
    );
    Ok(cfg)
}

pub fn proton_status(
    proton: &Map<String, Value>,
    gamescope: &Map<String, Value>,
    root: &Path,
) -> Result<Value> {
    let prefer_gamemode = prefer_gamemode(root)?;
    let env = proton_environment(proton)?;
    let prefix = proton_prefix_with_preference(proton, gamescope, prefer_gamemode)?;
    let exe = env::current_exe().ok();
    let sibling = exe
        .as_ref()
        .and_then(|e| e.parent())
        .map(|d| d.join("astrea-gaming"))
        .filter(|p| p.is_file() && is_executable(p))
        .or_else(|| find_executable("astrea-gaming"))
        .unwrap_or_else(|| PathBuf::from("astrea-gaming"));
    let data_home = data_home();
    let state_home = state_home();
    let proton_path = find_proton()
        .map(|p| p.display().to_string())
        .unwrap_or_default();
    let sibling_text = sibling.to_string_lossy().into_owned();
    let launch_command =
        shlex::try_join([sibling_text.as_str()]).unwrap_or_else(|_| "astrea-gaming".into());
    let env_json: BTreeMap<String, String> = env
        .iter()
        .map(|(k, v)| {
            (
                k.to_string_lossy().into_owned(),
                v.to_string_lossy().into_owned(),
            )
        })
        .collect();
    let prefix_json: Vec<String> = prefix
        .iter()
        .map(|p| p.to_string_lossy().into_owned())
        .collect();
    Ok(serde_json::json!({
        "capabilities":{"gamemode":find_executable("gamemoderun").is_some(),"mangohud":find_executable("mangohud").is_some(),"gamescope":find_executable("gamescope").is_some(),"wine":find_executable("wine").is_some(),"proton":!proton_path.is_empty()},
        "native_launcher":{"available":sibling.is_absolute()&&sibling.is_file(),"path":sibling},
        "launch_command":format!("{launch_command} %command%"),
        "proton_env":env_json,"proton_prefix":prefix_json,
        "prefer_gamemode":prefer_gamemode,
        "config_path":root.join("AstreaOS/gaming/proton.json"),
        "compatibility_config_path":root.join("AstreaOS/gaming/compatibility.json"),
        "gamescope_config_path":root.join("AstreaOS/gaming/gamescope.json"),
        "performance_config_path":root.join("AstreaOS/system/performance.json"),
        "prefix_path":data_home.join("AstreaOS/windows-prefixes/shared/proton/pfx"),
        "wine_prefix_path":data_home.join("AstreaOS/windows-prefixes/shared/wine"),
        "log_path":state_home.join("AstreaOS/windows-prefixes/logs"),"proton_path":proton_path,
        "proton_preset":proton.get("preset").and_then(Value::as_str).unwrap_or("recommended")
    }))
}

pub fn prefer_gamemode(root: &Path) -> Result<bool> {
    let doc = JsonStore::new(root.join("AstreaOS/system/performance.json")).read()?;
    match doc.get("prefer_gamemode") {
        None => Ok(true),
        Some(Value::Bool(value)) => Ok(*value),
        Some(_) => {
            Err("AstreaOS/system/performance.json: prefer_gamemode must be a boolean".into())
        }
    }
}

fn data_home() -> PathBuf {
    env::var_os("XDG_DATA_HOME")
        .map(PathBuf::from)
        .filter(|p| p.is_absolute())
        .or_else(|| {
            env::var_os("HOME")
                .map(PathBuf::from)
                .map(|p| p.join(".local/share"))
        })
        .unwrap_or_else(|| PathBuf::from(".local/share"))
}
fn state_home() -> PathBuf {
    env::var_os("XDG_STATE_HOME")
        .map(PathBuf::from)
        .filter(|p| p.is_absolute())
        .or_else(|| {
            env::var_os("HOME")
                .map(PathBuf::from)
                .map(|p| p.join(".local/state"))
        })
        .unwrap_or_else(|| PathBuf::from(".local/state"))
}
fn find_proton() -> Option<PathBuf> {
    if let Some(p) = env::var_os("ASTREA_PROTON")
        .map(PathBuf::from)
        .filter(|p| p.is_file() && is_executable(p))
    {
        return Some(p);
    }
    if let Some(p) = find_executable("proton") {
        return Some(p);
    }
    let home = env::var_os("HOME").map(PathBuf::from)?;
    for root in [
        home.join(".steam/steam/steamapps/common"),
        home.join(".local/share/Steam/steamapps/common"),
    ] {
        if let Ok(entries) = fs::read_dir(root) {
            for entry in entries.flatten().take(256) {
                let p = entry.path().join("proton");
                if p.is_file() && is_executable(&p) {
                    return Some(p);
                }
            }
        }
    }
    None
}

pub fn build_proton_plan(
    proton: &Map<String, Value>,
    gamescope: &Map<String, Value>,
    game: Vec<OsString>,
) -> Result<LaunchPlan> {
    let root = config_home()?;
    build_proton_plan_with_gamemode(proton, gamescope, game, prefer_gamemode(&root)?)
}
pub fn build_proton_plan_with_gamemode(
    proton: &Map<String, Value>,
    gamescope: &Map<String, Value>,
    game: Vec<OsString>,
    prefer_gamemode: bool,
) -> Result<LaunchPlan> {
    if game.is_empty() {
        return Err("no game command supplied".into());
    }
    let mut args = proton_prefix_with_preference(proton, gamescope, prefer_gamemode)?;
    args.extend(game);
    Ok(LaunchPlan {
        program: args.remove(0),
        args,
        env: proton_environment(proton)?,
    })
}

pub fn execute_plan(plan: LaunchPlan) -> std::io::Error {
    let mut cmd = Command::new(&plan.program);
    cmd.args(&plan.args).envs(&plan.env);
    cmd.exec()
}

pub fn build_compatibility_plan(
    service: &GamingService,
    executable: PathBuf,
    argv: Vec<OsString>,
) -> Result<LaunchPlan> {
    build_compatibility_plan_with_create(service, executable, argv, true)
}
pub fn build_compatibility_plan_with_create(
    service: &GamingService,
    executable: PathBuf,
    argv: Vec<OsString>,
    create_prefix: bool,
) -> Result<LaunchPlan> {
    let snapshot = service.snapshot()?;
    let compat = snapshot["compatibility"]
        .as_object()
        .ok_or("invalid compatibility snapshot")?;
    let proton = snapshot["proton"]
        .as_object()
        .ok_or("invalid Proton snapshot")?;
    let gamescope = snapshot["gamescope"]
        .as_object()
        .ok_or("invalid Gamescope snapshot")?;
    let use_profile = compat["use_proton_profile"].as_bool().unwrap_or(true);
    let mut envs = if use_profile {
        proton_environment(proton)?
    } else {
        BTreeMap::new()
    };
    if !use_profile {
        envs.extend(env_words(
            compat["extra_env"].as_str().unwrap_or(""),
            "extra_env",
        )?);
    }
    let runner = compat["runner"].as_str().unwrap_or("proton");
    let proton_bin = find_proton();
    let wine_bin = find_executable("wine");
    let selected = match runner {
        "wine" => wine_bin,
        "auto" => proton_bin.clone().or(wine_bin),
        _ => proton_bin.clone(),
    }
    .ok_or_else(|| format!("requested compatibility runner '{runner}' is unavailable"))?;
    let mut args = vec![selected.clone().into_os_string()];
    let is_proton = proton_bin.as_ref() == Some(&selected);
    let data_home = data_home();
    if is_proton {
        let logs = state_home().join("AstreaOS/windows-prefixes/logs");
        if create_prefix {
            fs::create_dir_all(&logs)
                .map_err(|e| format!("create Proton log directory {}: {e}", logs.display()))?;
        }
        envs.entry("PROTON_LOG_DIR".into())
            .or_insert_with(|| logs.into_os_string());
        let prefix = data_home.join("AstreaOS/windows-prefixes/shared/proton");
        if create_prefix {
            fs::create_dir_all(prefix.join("pfx"))
                .map_err(|e| format!("create Proton prefix {}: {e}", prefix.display()))?;
        }
        envs.insert("STEAM_COMPAT_DATA_PATH".into(), prefix.into_os_string());
        envs.insert(
            "STEAM_COMPAT_CLIENT_INSTALL_PATH".into(),
            data_home.join("Steam").into_os_string(),
        );
        args.extend(["run".into(), executable.into_os_string()]);
    } else {
        let prefix = data_home.join("AstreaOS/windows-prefixes/shared/wine");
        if create_prefix {
            fs::create_dir_all(&prefix)
                .map_err(|e| format!("create Wine prefix {}: {e}", prefix.display()))?;
        }
        envs.insert("WINEPREFIX".into(), prefix.into_os_string());
        args.push(executable.into_os_string());
    }
    args.extend(argv);
    let mut wrapped = Vec::new();
    let allow_gamemode = prefer_gamemode(service.config_root())?;
    if use_profile {
        wrapped = proton_prefix_with_preference(proton, gamescope, allow_gamemode)?;
    } else {
        if allow_gamemode
            && compat["gamemode"].as_bool() == Some(true)
            && let Some(p) = wrapper("gamemoderun")
        {
            wrapped.push(p);
        }
        let mut mango = compat["mangohud"].as_bool() == Some(true);
        if compat["gamescope"].as_bool() == Some(true)
            && let Some(gs) = wrapper("gamescope")
        {
            wrapped.extend(gamescope_argv(
                &proton_gamescope_config(proton, gamescope)?,
                mango,
                gs,
            )?);
            mango = false;
        }
        if mango && let Some(p) = wrapper("mangohud") {
            wrapped.push(p);
        }
        wrapped.extend(
            parse_words(
                compat["extra_prefix"].as_str().unwrap_or(""),
                "extra_prefix",
            )?
            .into_iter()
            .map(Into::into),
        );
    }
    wrapped.extend(args);
    let program = if wrapped.is_empty() {
        selected.into_os_string()
    } else {
        wrapped.remove(0)
    };
    Ok(LaunchPlan {
        program,
        args: wrapped,
        env: envs,
    })
}

pub fn default_service() -> Result<GamingService> {
    Ok(GamingService::new(config_home()?))
}
