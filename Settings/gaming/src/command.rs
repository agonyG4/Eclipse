//! Bounded, argv-only command execution for the gaming settings backend.

use crate::Result;
use std::{
    io::Read,
    path::{Path, PathBuf},
    process::{Child, Command, Stdio},
    sync::mpsc,
    thread,
    time::{Duration, Instant},
};

const COMMAND_TIMEOUT: Duration = Duration::from_secs(5);
const OUTPUT_LIMIT: usize = 64 * 1024;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CommandOutput {
    pub code: i32,
    pub stdout: String,
    pub stderr: String,
}

pub trait CommandRunner: Send + Sync {
    fn available(&self, program: &str) -> bool;
    fn run(&self, program: &str, args: &[String]) -> Result<CommandOutput>;
}

#[derive(Debug, Default, Clone, Copy)]
pub struct NativeCommandRunner;

impl CommandRunner for NativeCommandRunner {
    fn available(&self, program: &str) -> bool {
        find_program(program).is_some()
    }

    fn run(&self, program: &str, args: &[String]) -> Result<CommandOutput> {
        let executable = find_program(program)
            .ok_or_else(|| format!("{program} is not installed or is not executable"))?;
        let mut command = Command::new(executable);
        command
            .args(args)
            .stdin(Stdio::null())
            .stdout(Stdio::piped())
            .stderr(Stdio::piped());
        #[cfg(unix)]
        {
            use std::os::unix::process::CommandExt;
            command.process_group(0);
        }
        let mut child = command
            .spawn()
            .map_err(|e| format!("could not start {program}: {e}"))?;
        let Some(stdout) = child.stdout.take() else {
            terminate_group(&mut child);
            return Err(format!("{program} did not provide its piped stdout"));
        };
        let Some(stderr) = child.stderr.take() else {
            terminate_group(&mut child);
            return Err(format!("{program} did not provide its piped stderr"));
        };
        let (sender, receiver) = mpsc::channel();
        let out_sender = sender.clone();
        let out_reader = match thread::Builder::new()
            .name("gaming-command-stdout".into())
            .spawn(move || {
                let _ = out_sender.send((0, drain_bounded(stdout, OUTPUT_LIMIT)));
            }) {
            Ok(reader) => reader,
            Err(error) => {
                terminate_group(&mut child);
                return Err(format!("could not start {program} stdout reader: {error}"));
            }
        };
        if let Err(error) = thread::Builder::new()
            .name("gaming-command-stderr".into())
            .spawn(move || {
                let _ = sender.send((1, drain_bounded(stderr, OUTPUT_LIMIT)));
            })
        {
            terminate_group(&mut child);
            let _ = out_reader.join();
            return Err(format!("could not start {program} stderr reader: {error}"));
        }

        let deadline = Instant::now() + COMMAND_TIMEOUT;
        let code = loop {
            match child.try_wait() {
                Ok(Some(status)) => break status.code().unwrap_or(128),
                Ok(None) if Instant::now() < deadline => thread::sleep(Duration::from_millis(10)),
                Ok(None) => {
                    terminate_group(&mut child);
                    let _ = collect_readers(&receiver, &mut child, Duration::ZERO);
                    return Err(format!(
                        "{program} timed out after {} seconds",
                        COMMAND_TIMEOUT.as_secs()
                    ));
                }
                Err(e) => {
                    terminate_group(&mut child);
                    let _ = collect_readers(&receiver, &mut child, Duration::ZERO);
                    return Err(format!("could not wait for {program}: {e}"));
                }
            }
        };

        let (stdout, stderr) = collect_readers(&receiver, &mut child, Duration::from_millis(250))?;
        Ok(CommandOutput {
            code,
            stdout,
            stderr,
        })
    }
}

fn collect_readers(
    receiver: &mpsc::Receiver<(usize, std::io::Result<Vec<u8>>)>,
    child: &mut Child,
    graceful: Duration,
) -> Result<(String, String)> {
    let mut output: [Option<Vec<u8>>; 2] = [None, None];
    let mut killed_descendants = graceful.is_zero();
    loop {
        if output.iter().all(Option::is_some) {
            break;
        }
        let timeout = if killed_descendants {
            Duration::from_secs(1)
        } else {
            graceful
        };
        let deadline = Instant::now() + timeout;
        while output.iter().any(Option::is_none) {
            let remaining = deadline.saturating_duration_since(Instant::now());
            if remaining.is_zero() {
                break;
            }
            match receiver.recv_timeout(remaining) {
                Ok((index, bytes)) => {
                    output[index] =
                        Some(bytes.map_err(|e| format!("could not read command output: {e}"))?);
                }
                Err(_) => break,
            }
        }
        if output.iter().all(Option::is_some) {
            break;
        }
        if killed_descendants {
            return Err("timed out draining command output after terminating descendants".into());
        }
        // A descendant may have inherited one of the pipes. Bound this case too.
        terminate_group(child);
        killed_descendants = true;
    }
    let stdout = String::from_utf8_lossy(output[0].as_deref().unwrap_or_default()).into_owned();
    let stderr = String::from_utf8_lossy(output[1].as_deref().unwrap_or_default()).into_owned();
    Ok((stdout, stderr))
}

fn drain_bounded(mut reader: impl Read, limit: usize) -> std::io::Result<Vec<u8>> {
    let mut kept = Vec::with_capacity(limit.min(8192));
    let mut buffer = [0_u8; 8192];
    loop {
        let count = reader.read(&mut buffer)?;
        if count == 0 {
            break;
        }
        let remaining = limit.saturating_sub(kept.len());
        kept.extend_from_slice(&buffer[..count.min(remaining)]);
    }
    Ok(kept)
}

fn terminate_group(child: &mut Child) {
    #[cfg(unix)]
    {
        // The command is the leader of a fresh process group. Kill descendants too,
        // so inherited pipe handles cannot keep the reader threads alive.
        // SAFETY: kill receives a process-group id and a constant signal.
        unsafe {
            libc::kill(-(child.id() as i32), libc::SIGKILL);
        }
    }
    let _ = child.kill();
    let _ = child.wait();
}

fn find_program(program: &str) -> Option<PathBuf> {
    let candidate = Path::new(program);
    if candidate.components().count() > 1 {
        return is_executable(candidate).then(|| candidate.to_path_buf());
    }
    let path = std::env::var_os("PATH")?;
    std::env::split_paths(&path)
        .map(|dir| dir.join(program))
        .find(|candidate| is_executable(candidate))
}

#[cfg(unix)]
fn is_executable(path: &Path) -> bool {
    use std::os::unix::fs::PermissionsExt;
    std::fs::metadata(path)
        .map(|meta| meta.is_file() && meta.permissions().mode() & 0o111 != 0)
        .unwrap_or(false)
}

#[cfg(not(unix))]
fn is_executable(path: &Path) -> bool {
    path.is_file()
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::{fs, os::unix::fs::PermissionsExt};

    fn script(dir: &Path, name: &str, body: &str) -> PathBuf {
        let path = dir.join(name);
        fs::write(&path, format!("#!/bin/sh\n{body}\n")).unwrap();
        fs::set_permissions(&path, fs::Permissions::from_mode(0o700)).unwrap();
        path
    }

    #[test]
    fn runs_argv_literally_and_captures_bounded_nonzero_output() {
        let dir = std::env::temp_dir().join(format!("astrea-command-{}", std::process::id()));
        fs::create_dir_all(&dir).unwrap();
        let path = script(
            &dir,
            "capture",
            "printf '%s\\n' \"$1\"; printf 'bad\\n' >&2; exit 7",
        );
        let arg = "two words; $(touch /tmp/should-not-exist)".to_string();
        let output = NativeCommandRunner
            .run(path.to_str().unwrap(), std::slice::from_ref(&arg))
            .unwrap();
        assert_eq!(output.code, 7);
        assert_eq!(output.stdout.trim(), arg);
        assert_eq!(output.stderr.trim(), "bad");
        let _ = fs::remove_dir_all(dir);
    }

    #[test]
    fn drains_large_output_but_keeps_only_the_limit() {
        let dir = std::env::temp_dir().join(format!("astrea-command-large-{}", std::process::id()));
        fs::create_dir_all(&dir).unwrap();
        let path = script(
            &dir,
            "large",
            "yes x | head -c 200000; yes y | head -c 200000 >&2",
        );
        let output = NativeCommandRunner
            .run(path.to_str().unwrap(), &[])
            .unwrap();
        assert_eq!(output.stdout.len(), OUTPUT_LIMIT);
        assert_eq!(output.stderr.len(), OUTPUT_LIMIT);
        let _ = fs::remove_dir_all(dir);
    }

    #[test]
    fn kills_and_reaps_timed_out_commands() {
        let dir =
            std::env::temp_dir().join(format!("astrea-command-timeout-{}", std::process::id()));
        fs::create_dir_all(&dir).unwrap();
        let path = script(&dir, "slow", "sleep 10");
        let started = Instant::now();
        let err = NativeCommandRunner
            .run(path.to_str().unwrap(), &[])
            .unwrap_err();
        assert!(
            err.contains("timed out"),
            "unexpected timeout result: {err}"
        );
        assert!(started.elapsed() < Duration::from_secs(7));
        let _ = fs::remove_dir_all(dir);
    }

    #[test]
    fn bounds_pipe_drain_when_a_descendant_keeps_descriptors_open() {
        let dir =
            std::env::temp_dir().join(format!("astrea-command-descendant-{}", std::process::id()));
        fs::create_dir_all(&dir).unwrap();
        let path = script(&dir, "descendant", "sleep 10 & exit 0");
        let started = Instant::now();
        let result = NativeCommandRunner.run(path.to_str().unwrap(), &[]);
        assert!(result.is_ok());
        assert!(started.elapsed() < Duration::from_secs(3));
        let _ = fs::remove_dir_all(dir);
    }
}
