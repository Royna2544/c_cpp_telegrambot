use std::{path::PathBuf, process::Stdio, sync::Arc};

use async_trait::async_trait;
#[cfg(unix)]
use nix::{
    sys::signal::{self, Signal},
    unistd::Pid,
};
use tokio::{
    io::{AsyncBufReadExt, AsyncWriteExt, BufReader},
    process::Command,
    sync::{Mutex, mpsc},
};
use tracing::{error, info};

use super::{BuildError, BuildEvent, BuildOutcome, BuildTerminalState};

#[async_trait]
pub trait BuildEventSender: Send + Sync {
    async fn send(&self, event: BuildEvent);
}

pub struct CommandSpec {
    pub command: Command,
    pub log_path: Option<PathBuf>,
    pub stdin_rx: Option<mpsc::Receiver<String>>,
}

impl CommandSpec {
    pub fn new(command: Command) -> Self {
        Self {
            command,
            log_path: None,
            stdin_rx: None,
        }
    }

    pub fn with_log_path(mut self, log_path: Option<PathBuf>) -> Self {
        self.log_path = log_path;
        self
    }

    pub fn with_stdin(mut self, stdin_rx: Option<mpsc::Receiver<String>>) -> Self {
        self.stdin_rx = stdin_rx;
        self
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub struct CommandOutcome {
    pub success: bool,
    pub exit_code: Option<i32>,
    pub cancelled: bool,
}

#[async_trait]
pub trait ProcessRunner: Send + Sync {
    async fn run(
        &self,
        spec: CommandSpec,
        events: Arc<dyn BuildEventSender>,
        kill_rx: Option<&mut mpsc::Receiver<()>>,
    ) -> Result<CommandOutcome, BuildError>;

    fn program_available(&self, program: &str) -> bool {
        which::which(program).is_ok()
    }
}

pub struct RealProcessRunner;

#[async_trait]
impl ProcessRunner for RealProcessRunner {
    async fn run(
        &self,
        mut spec: CommandSpec,
        events: Arc<dyn BuildEventSender>,
        mut kill_rx: Option<&mut mpsc::Receiver<()>>,
    ) -> Result<CommandOutcome, BuildError> {
        let file_handle = if let Some(path) = &spec.log_path {
            let file = tokio::fs::File::create(path).await?;
            Some(Arc::new(Mutex::new(file)))
        } else {
            None
        };

        #[cfg(unix)]
        spec.command.process_group(0);
        spec.command.kill_on_drop(true);
        spec.command.stdout(Stdio::piped());
        spec.command.stderr(Stdio::piped());
        if spec.stdin_rx.is_some() {
            spec.command.stdin(Stdio::piped());
        } else {
            spec.command.stdin(Stdio::null());
        }

        let program = spec
            .command
            .as_std()
            .get_program()
            .to_string_lossy()
            .into_owned();
        let args = spec
            .command
            .as_std()
            .get_args()
            .map(|arg| arg.to_string_lossy().into_owned())
            .collect::<Vec<_>>();
        let working_dir = spec
            .command
            .as_std()
            .get_current_dir()
            .map(|path| path.display().to_string())
            .unwrap_or_else(|| "<unknown>".into());

        let mut child = spec
            .command
            .spawn()
            .map_err(|error| BuildError::Internal(error.to_string()))?;
        info!(
            "Spawned command: {} {}, wd: {} (PID: {:?})",
            program,
            args.join(" "),
            working_dir,
            child.id()
        );

        if let Some(mut stdin_rx) = spec.stdin_rx {
            if let Some(mut child_stdin) = child.stdin.take() {
                tokio::spawn(async move {
                    while let Some(message) = stdin_rx.recv().await {
                        if child_stdin.write_all(message.as_bytes()).await.is_err() {
                            break;
                        }
                        if !message.ends_with('\n') && child_stdin.write_all(b"\n").await.is_err() {
                            break;
                        }
                        if child_stdin.flush().await.is_err() {
                            break;
                        }
                    }
                });
            }
        }

        let stdout = child
            .stdout
            .take()
            .ok_or_else(|| BuildError::Internal("Child stdout is unavailable".into()))?;
        let stderr = child
            .stderr
            .take()
            .ok_or_else(|| BuildError::Internal("Child stderr is unavailable".into()))?;

        let stdout_events = events.clone();
        let stdout_file = file_handle.clone();
        let stdout_task = tokio::spawn(async move {
            let mut first = true;
            let mut lines = BufReader::new(stdout).lines();
            while let Ok(Some(line)) = lines.next_line().await {
                if let Some(file) = &stdout_file {
                    let mut file = file.lock().await;
                    let _ = file.write_all(line.as_bytes()).await;
                    let _ = file.write_all(b"\n").await;
                }
                stdout_events
                    .send(BuildEvent::Stdout {
                        line,
                        first,
                        at: chrono::Utc::now(),
                    })
                    .await;
                first = false;
            }
        });

        let stderr_events = events.clone();
        let stderr_file = file_handle.clone();
        let stderr_task = tokio::spawn(async move {
            let mut first = true;
            let mut lines = BufReader::new(stderr).lines();
            while let Ok(Some(line)) = lines.next_line().await {
                if let Some(file) = &stderr_file {
                    let mut file = file.lock().await;
                    let _ = file.write_all(format!("ERR: {line}\n").as_bytes()).await;
                }
                stderr_events
                    .send(BuildEvent::Stderr {
                        line,
                        first,
                        at: chrono::Utc::now(),
                    })
                    .await;
                first = false;
            }
        });

        let (status, cancelled) = if let Some(receiver) = &mut kill_rx {
            tokio::select! {
                status = child.wait() => (status?, false),
                _ = receiver.recv() => {
                    #[cfg(unix)]
                    if let Some(pid) = child.id() {
                        let _ = signal::kill(Pid::from_raw(-(pid as i32)), Signal::SIGINT);
                    }
                    #[cfg(not(unix))]
                    let _ = child.kill().await;

                    let exited = tokio::time::timeout(
                        std::time::Duration::from_secs(10),
                        child.wait(),
                    )
                    .await;
                    if exited.is_err() {
                        #[cfg(unix)]
                        if let Some(pid) = child.id() {
                            let _ = signal::kill(Pid::from_raw(-(pid as i32)), Signal::SIGKILL);
                        }
                        let _ = child.kill().await;
                        let _ = child.wait().await;
                    }
                    events.send(BuildEvent::Finished {
                        outcome: BuildOutcome {
                            state: BuildTerminalState::Cancelled,
                            message: Some("Build cancelled".into()),
                        },
                        at: chrono::Utc::now(),
                    }).await;
                    stdout_task.abort();
                    stderr_task.abort();
                    if let Some(file) = &file_handle {
                        let mut file = file.lock().await;
                        let _ = file.write_all(b"\n--- BUILD CANCELLED ---\n").await;
                    }
                    return Ok(CommandOutcome {
                        success: false,
                        exit_code: None,
                        cancelled: true,
                    });
                }
            }
        } else {
            (child.wait().await?, false)
        };

        let _ = stdout_task.await;
        let _ = stderr_task.await;
        if !status.success() {
            error!("Process exited with status: {status}");
        }
        Ok(CommandOutcome {
            success: status.success(),
            exit_code: status.code(),
            cancelled,
        })
    }
}
