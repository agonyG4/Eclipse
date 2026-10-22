use astrea_gaming::{config::GamingService, store::JsonStore};
use serde_json::{Value, json};
use std::{fs, os::unix::fs::PermissionsExt, path::PathBuf, process::Command};

fn home() -> PathBuf {
    tempfile::tempdir().unwrap().keep()
}

#[test]
fn missing_store_reads_empty_and_updates_preserve_unknown_keys() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("proton.json");
    let store = JsonStore::new(path.clone());
    assert!(store.read().unwrap().is_empty());
    assert!(!path.exists(), "read must not initialize a document");
    fs::write(&path, r#"{"future_option":{"x":1},"gamemode":true}"#).unwrap();
    store
        .update(|doc| {
            doc.insert("mangohud".into(), json!(true));
            Ok(())
        })
        .unwrap();
    let saved: Value = serde_json::from_slice(&fs::read(&path).unwrap()).unwrap();
    assert_eq!(saved["future_option"]["x"], 1);
    assert_eq!(saved["mangohud"], true);
}

#[test]
fn malformed_store_is_reported_without_replacing_bytes() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("bad.json");
    let bytes = b"{not json";
    fs::write(&path, bytes).unwrap();
    let store = JsonStore::new(path.clone());
    assert!(store.read().is_err());
    assert!(store.update(|_| Ok(())).is_err());
    assert_eq!(fs::read(path).unwrap(), bytes);
}

#[test]
fn concurrent_store_patches_do_not_lose_updates() {
    let dir = tempfile::tempdir().unwrap();
    let path = dir.path().join("counter.json");
    let mut workers = Vec::new();
    for _ in 0..8 {
        let path = path.clone();
        workers.push(std::thread::spawn(move || {
            for _ in 0..20 {
                JsonStore::new(path.clone())
                    .update(|doc| {
                        let value = doc.get("count").and_then(Value::as_u64).unwrap_or(0) + 1;
                        doc.insert("count".into(), json!(value));
                        Ok(())
                    })
                    .unwrap();
            }
        }));
    }
    for worker in workers {
        worker.join().unwrap();
    }
    assert_eq!(JsonStore::new(path).read().unwrap()["count"], 160);
}

#[test]
fn proton_defaults_and_legacy_sync_migrate_in_memory_without_read_writes() {
    let root = home();
    let proton_path = root.join("AstreaOS/gaming/proton.json");
    fs::create_dir_all(proton_path.parent().unwrap()).unwrap();
    fs::write(
        &proton_path,
        r#"{"config_version":1,"enable_esync":false,"enable_fsync":true,"unknown":"kept"}"#,
    )
    .unwrap();
    let service = GamingService::new(root.clone());
    let snapshot = service.snapshot().unwrap();
    assert_eq!(snapshot["proton"]["sync_mode"], "disable-esync");
    assert_eq!(snapshot["proton"]["config_version"], 2);
    assert_eq!(snapshot["proton"]["unknown"], "kept");
    let saved: Value = serde_json::from_slice(&fs::read(&proton_path).unwrap()).unwrap();
    assert_eq!(
        saved["config_version"], 1,
        "snapshot must not migrate on disk"
    );
}

#[test]
fn preset_and_individual_updates_are_validated_and_preserve_custom_options() {
    let root = home();
    let service = GamingService::new(root.clone());
    service.set_proton("custom_env", json!("FOO=bar")).unwrap();
    let out = service.set_proton("preset", json!("nvidia")).unwrap();
    assert_eq!(out["proton"]["preset"], "nvidia");
    assert_eq!(out["proton"]["enable_nvapi"], true);
    assert_eq!(
        out["proton"]["custom_env"], "",
        "preset clears legacy custom env"
    );
    service.set_proton("custom_env", json!("FOO=bar")).unwrap();
    let out = service.set_proton("fsr_strength", json!(4)).unwrap();
    assert_eq!(out["proton"]["preset"], "custom");
    assert_eq!(out["proton"]["custom_env"], "FOO=bar");
    assert!(service.set_proton("fsr_strength", json!(9)).is_err());
    assert!(service.set_proton("gamemode", json!("yes")).is_err());
    assert!(
        service
            .set_proton("custom_env", json!("BROKEN='unterminated"))
            .is_err()
    );
    assert!(service.set_proton("unknown", json!(true)).is_err());
    assert!(service.set_proton("custom_env", json!("FOO=a\0b")).is_err());
}

#[test]
fn invalid_values_do_not_create_configuration_files_or_directories() {
    let root = tempfile::tempdir().unwrap();
    let service = GamingService::new(root.path().to_path_buf());
    assert!(service.set_proton("gamescope_width", json!(100)).is_err());
    assert!(
        service
            .set_compatibility("runner", json!("invalid"))
            .is_err()
    );
    assert!(!root.path().join("AstreaOS").exists());
}

#[test]
fn invalid_existing_config_is_never_clobbered_by_snapshot_or_mutation() {
    let root = home();
    let path = root.join("AstreaOS/gaming/compatibility.json");
    fs::create_dir_all(path.parent().unwrap()).unwrap();
    let bytes = b"[]";
    fs::write(&path, bytes).unwrap();
    let service = GamingService::new(root);
    assert!(service.snapshot().is_err());
    assert!(service.set_compatibility("runner", json!("wine")).is_err());
    assert_eq!(fs::read(path).unwrap(), bytes);
}

#[test]
fn launch_plans_keep_shell_metacharacters_and_spaces_as_literal_arguments() {
    use astrea_gaming::launch::build_proton_plan;
    let proton =
        json!({"custom_env":"TOKEN='value with spaces'", "gamemode":false, "custom_prefix":""})
            .as_object()
            .unwrap()
            .clone();
    let gamescope = json!({}).as_object().unwrap().clone();
    let argv = vec![
        "/tmp/game with spaces".into(),
        "$(touch /tmp/should-not-exist)".into(),
        "a;b".into(),
    ];
    let plan = build_proton_plan(&proton, &gamescope, argv.clone()).unwrap();
    assert_eq!(plan.program, argv[0]);
    assert_eq!(plan.args, &argv[1..]);
    assert_eq!(plan.env[std::ffi::OsStr::new("TOKEN")], "value with spaces");
}

#[test]
fn gamescope_wraps_mangohud_with_shared_profile_arguments() {
    use astrea_gaming::launch::proton_prefix_with_tools;
    let dir = tempfile::tempdir().unwrap();
    let gamescope = dir.path().join("gamescope");
    fs::write(&gamescope, "#!/bin/sh\nexit 0\n").unwrap();
    fs::set_permissions(&gamescope, fs::Permissions::from_mode(0o755)).unwrap();
    let gamemode = dir.path().join("gamemoderun");
    fs::write(&gamemode, "#!/bin/sh\nexit 0\n").unwrap();
    fs::set_permissions(&gamemode, fs::Permissions::from_mode(0o755)).unwrap();
    let cfg = json!({"gamemode":true,"gamescope":true,"mangohud":true,"custom_prefix":""})
        .as_object()
        .unwrap()
        .clone();
    let gs=json!({"fullscreen":true,"width":1280,"height":720,"refresh":60,"immediate_flips":true,"hide_cursor":false,"extra_args":"--adaptive-sync"}).as_object().unwrap().clone();
    let prefix = proton_prefix_with_tools(
        &cfg,
        &gs,
        true,
        Some(gamemode.clone().into_os_string()),
        Some(gamescope.clone().into_os_string()),
        None,
    )
    .unwrap();
    let printable: Vec<String> = prefix
        .iter()
        .map(|x| x.to_string_lossy().into_owned())
        .collect();
    assert_eq!(printable[0], gamemode.to_string_lossy());
    assert_eq!(printable[1], gamescope.to_string_lossy());
    assert!(printable.contains(&"--mangoapp".to_string()));
    assert!(printable.contains(&"--adaptive-sync".to_string()));
    assert_eq!(printable.last().unwrap(), "--");
}

#[test]
fn manual_proton_gamescope_profile_uses_manual_dimensions_and_flags() {
    use astrea_gaming::launch::{gamescope_argv, proton_gamescope_config};
    let proton=json!({"use_gamescope_profile":false,"gamescope_width":1600,"gamescope_height":900,"gamescope_refresh":144,"gamescope_fullscreen":false,"gamescope_immediate_flips":true,"gamescope_hide_cursor":true,"gamescope_force_grab_cursor":true,"gamescope_adaptive_sync":true,"gamescope_extra_args":"--nested-refresh 120"}).as_object().unwrap().clone();
    let shared = json!({"width":1920,"height":1080,"refresh":165,"fullscreen":true})
        .as_object()
        .unwrap()
        .clone();
    let effective = proton_gamescope_config(&proton, &shared).unwrap();
    let argv: Vec<String> = gamescope_argv(&effective, true, "gamescope".into())
        .unwrap()
        .iter()
        .map(|x| x.to_string_lossy().into_owned())
        .collect();
    assert!(argv.contains(&"1600".into()));
    assert!(argv.contains(&"900".into()));
    assert!(argv.contains(&"144".into()));
    assert!(argv.contains(&"--mangoapp".into()));
    assert!(argv.contains(&"--force-grab-cursor".into()));
    assert!(argv.contains(&"--adaptive-sync".into()));
    assert!(!argv.contains(&"-f".into()));
}

#[test]
fn performance_preference_gates_the_per_game_gamemode_wrapper() {
    use astrea_gaming::launch::{prefer_gamemode, proton_prefix_with_tools};
    let root = tempfile::tempdir().unwrap();
    let path = root.path().join("AstreaOS/system/performance.json");
    fs::create_dir_all(path.parent().unwrap()).unwrap();
    fs::write(&path, r#"{"prefer_gamemode":false}"#).unwrap();
    assert!(!prefer_gamemode(root.path()).unwrap());
    let cfg = json!({"gamemode":true,"gamescope":false,"mangohud":false,"custom_prefix":""})
        .as_object()
        .unwrap()
        .clone();
    let shared = json!({}).as_object().unwrap().clone();
    let wrapped = proton_prefix_with_tools(
        &cfg,
        &shared,
        prefer_gamemode(root.path()).unwrap(),
        Some("gamemoderun".into()),
        None,
        None,
    )
    .unwrap();
    assert!(wrapped.is_empty());
    fs::write(path, r#"{"prefer_gamemode":"yes"}"#).unwrap();
    assert!(prefer_gamemode(root.path()).is_err());
}

#[test]
fn compatibility_plan_inherits_proton_environment_and_resolves_wine() {
    use std::process::Command;
    let root = tempfile::tempdir().unwrap();
    let dir = tempfile::tempdir().unwrap();
    let wine = dir.path().join("wine");
    fs::write(&wine, "#!/bin/sh\nexit 0\n").unwrap();
    fs::set_permissions(&wine, fs::Permissions::from_mode(0o755)).unwrap();
    let compat_dir = root.path().join("AstreaOS/gaming");
    fs::create_dir_all(&compat_dir).unwrap();
    fs::write(compat_dir.join("compatibility.json"),r#"{"runner":"wine","use_proton_profile":true,"gamemode":false,"mangohud":false,"gamescope":false,"extra_env":"CUSTOM=value","extra_prefix":""}"#).unwrap();
    fs::write(
        compat_dir.join("proton.json"),
        r#"{"hide_nvidia_gpu":true,"gamemode":false,"custom_env":"PROFILE=kept"}"#,
    )
    .unwrap();
    let out = Command::new(env!("CARGO_BIN_EXE_astrea-gaming"))
        .env("ASTREA_GAMING_CONFIG_HOME", root.path())
        .env("XDG_DATA_HOME", root.path().join("data"))
        .env("PATH", dir.path())
        .args(["--dry-run", "--compat", "game.exe", "one two"])
        .output()
        .unwrap();
    assert!(
        out.status.success(),
        "{}",
        String::from_utf8_lossy(&out.stderr)
    );
    let plan: Value = serde_json::from_slice(&out.stdout).unwrap();
    assert_eq!(plan["argv"][0], wine.to_string_lossy().as_ref());
    assert_eq!(plan["argv"][1], "game.exe");
    assert_eq!(plan["argv"][2], "one two");
    assert_eq!(plan["env"]["PROTON_HIDE_NVIDIA_GPU"], "1");
    assert_eq!(plan["env"]["PROFILE"], "kept");
    assert!(
        plan["env"].get("CUSTOM").is_none(),
        "disabled custom options must not override the Proton profile"
    );
    assert!(
        !root
            .path()
            .join("AstreaOS/windows-prefixes/shared/wine")
            .exists(),
        "dry planning must not create prefix"
    );
}

#[test]
fn cli_dry_run_is_json_and_real_exec_preserves_child_status() {
    let root = tempfile::tempdir().unwrap();
    let fake = root.path().join("fake-game");
    fs::write(&fake, "#!/bin/sh\nexit 37\n").unwrap();
    fs::set_permissions(&fake, fs::Permissions::from_mode(0o755)).unwrap();
    let pref_path = root.path().join("AstreaOS/system/performance.json");
    fs::create_dir_all(pref_path.parent().unwrap()).unwrap();
    fs::write(pref_path, r#"{"prefer_gamemode":false}"#).unwrap();
    let bin = env!("CARGO_BIN_EXE_astrea-gaming");
    let dry = Command::new(bin)
        .env_remove("HOME")
        .env_remove("XDG_CONFIG_HOME")
        .env("ASTREA_GAMING_CONFIG_HOME", root.path())
        .args(["--dry-run", "--"])
        .arg(&fake)
        .arg("space ; $HOME")
        .output()
        .unwrap();
    assert!(dry.status.success());
    let value: Value = serde_json::from_slice(&dry.stdout).unwrap();
    assert_eq!(value["argv"][0], fake.to_string_lossy().as_ref());
    assert_eq!(value["argv"][1], "space ; $HOME");
    let run = Command::new(bin)
        .env("ASTREA_GAMING_CONFIG_HOME", root.path())
        .arg(&fake)
        .arg("quoted arg")
        .status()
        .unwrap();
    assert_eq!(run.code(), Some(37));
}

#[test]
fn compatibility_reuses_proton_wrappers_and_manual_gamescope_profile() {
    let root = tempfile::tempdir().unwrap();
    let bin = root.path().join("bin");
    fs::create_dir_all(&bin).unwrap();
    for name in ["wine", "proton", "gamemoderun", "gamescope", "mangohud"] {
        let path = bin.join(name);
        fs::write(&path, "#!/bin/sh\nexit 0\n").unwrap();
        fs::set_permissions(path, fs::Permissions::from_mode(0o755)).unwrap();
    }
    let config = root.path().join("AstreaOS/gaming");
    fs::create_dir_all(&config).unwrap();
    fs::write(config.join("compatibility.json"), r#"{"runner":"auto","use_proton_profile":true,"gamemode":false,"gamescope":false,"extra_prefix":"ignored"}"#).unwrap();
    fs::write(config.join("proton.json"), r#"{"gamemode":true,"gamescope":true,"mangohud":true,"use_gamescope_profile":false,"gamescope_width":1600,"custom_prefix":"profile-wrapper"}"#).unwrap();
    let output = Command::new(env!("CARGO_BIN_EXE_astrea-gaming"))
        .env("ASTREA_GAMING_CONFIG_HOME", root.path())
        .env("PATH", &bin)
        .args(["--dry-run", "--compat", "game.exe"])
        .output()
        .unwrap();
    assert!(
        output.status.success(),
        "{}",
        String::from_utf8_lossy(&output.stderr)
    );
    let plan: Value = serde_json::from_slice(&output.stdout).unwrap();
    let args = plan["argv"].as_array().unwrap();
    assert_eq!(args[0], bin.join("gamemoderun").to_string_lossy().as_ref());
    assert_eq!(args[1], bin.join("gamescope").to_string_lossy().as_ref());
    assert!(args.contains(&json!("1600")));
    assert!(args.contains(&json!("--mangoapp")));
    assert!(args.contains(&json!("profile-wrapper")));
    assert!(!args.contains(&json!("ignored")));
    assert!(args.contains(&json!(bin.join("proton").to_string_lossy())));
    assert!(args.contains(&json!("run")));
}
