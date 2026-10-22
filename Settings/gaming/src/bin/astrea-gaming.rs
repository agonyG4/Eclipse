use astrea_gaming::{
    config::GamingService,
    launch::{self, LaunchPlan},
    store::config_home,
};
use serde_json::json;
use std::{env, ffi::OsString, path::PathBuf, process};

fn usage() {
    println!(
        "Usage: astrea-gaming [--dry-run] [--] <command> [args...]\n       astrea-gaming [--dry-run] --compat <exe> [args...]\n\nLaunches a command using AstreaOS Proton and compatibility settings."
    );
}
fn printable(plan: &LaunchPlan) -> serde_json::Value {
    let args: Vec<String> = std::iter::once(plan.program.clone())
        .chain(plan.args.clone())
        .map(|s| s.to_string_lossy().into_owned())
        .collect();
    let env: serde_json::Map<String, serde_json::Value> = plan
        .env
        .iter()
        .map(|(k, v)| (k.to_string_lossy().into_owned(), json!(v.to_string_lossy())))
        .collect();
    json!({"argv":args,"env":env})
}
fn main() {
    let mut args = env::args_os().skip(1).peekable();
    let mut dry = false;
    while args.peek().is_some() {
        match args.peek().and_then(|a| a.to_str()) {
            Some("--help" | "-h") => {
                usage();
                return;
            }
            Some("--dry-run") => {
                args.next();
                dry = true
            }
            _ => break,
        }
    }
    let compat = if args.peek().and_then(|a| a.to_str()) == Some("--compat") {
        args.next();
        true
    } else {
        false
    };
    if args.peek().and_then(|a| a.to_str()) == Some("--") {
        args.next();
    }
    let Some(first) = args.next() else {
        eprintln!("astrea-gaming: no command supplied; use --help");
        process::exit(2);
    };
    let tail: Vec<OsString> = args.collect();
    let result = (|| -> Result<LaunchPlan, String> {
        let root = match env::var_os("ASTREA_GAMING_CONFIG_HOME") {
            Some(path) => PathBuf::from(path),
            None => config_home()?,
        };
        let service = GamingService::new(root);
        if compat {
            launch::build_compatibility_plan_with_create(&service, PathBuf::from(first), tail, !dry)
        } else {
            let snapshot = service.snapshot()?;
            let p = snapshot["proton"]
                .as_object()
                .ok_or("invalid Proton config")?;
            let g = snapshot["gamescope"]
                .as_object()
                .ok_or("invalid Gamescope config")?;
            launch::build_proton_plan_with_gamemode(
                p,
                g,
                std::iter::once(first).chain(tail).collect(),
                launch::prefer_gamemode(service.config_root())?,
            )
        }
    })();
    match result {
        Ok(plan) if dry => println!("{}", printable(&plan)),
        Ok(plan) => {
            let error = launch::execute_plan(plan);
            eprintln!("astrea-gaming: exec failed: {error}");
            process::exit(127)
        }
        Err(error) => {
            eprintln!("astrea-gaming: {error}");
            process::exit(2)
        }
    }
}
