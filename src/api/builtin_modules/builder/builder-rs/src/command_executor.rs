/// Command execution abstraction for testing
///
/// This module provides a trait-based abstraction for executing commands,
/// allowing the build services to be tested without actually spawning processes.
use async_trait::async_trait;
use std::path::PathBuf;
use std::process::{ExitStatus, Stdio};
use std::sync::Arc;
use tokio::io::{AsyncBufReadExt, BufReader};
use tokio::process::Command;
use tokio::sync::Mutex;
use tokio::sync::mpsc;
use tracing::{debug, error, info};

/// Result of executing a command
#[derive(Debug, Clone)]
pub struct CommandResult {
    pub success: bool,
    pub exit_code: Option<i32>,
    pub stdout_lines: Vec<String>,
    pub stderr_lines: Vec<String>,
}

/// Trait for executing commands
///
/// This trait abstracts the execution of external commands, allowing
/// for both real process execution and mock implementations for testing.
#[async_trait]
pub trait CommandExecutor: Send + Sync {
    /// Execute a command and stream output lines
    ///
    /// # Arguments
    /// * `program` - The program to execute
    /// * `args` - Command line arguments
    /// * `cwd` - Working directory (None for current directory)
    /// * `env` - Environment variables
    /// * `output_tx` - Channel to send output lines (format: "stdout: line" or "stderr: line")
    /// * `kill_rx` - Optional channel to receive kill signal
    ///
    /// # Returns
    /// CommandResult with execution status and captured output
    async fn execute(
        &self,
        program: &str,
        args: &[&str],
        cwd: Option<PathBuf>,
        env: &[(String, String)],
        output_tx: mpsc::Sender<String>,
        kill_rx: Option<mpsc::Receiver<()>>,
    ) -> Result<CommandResult, String>;
}

/// Real command executor that spawns actual processes
pub struct RealCommandExecutor;

impl RealCommandExecutor {
    pub fn new() -> Self {
        Self
    }
}

#[async_trait]
impl CommandExecutor for RealCommandExecutor {
    async fn execute(
        &self,
        program: &str,
        args: &[&str],
        cwd: Option<PathBuf>,
        env: &[(String, String)],
        output_tx: mpsc::Sender<String>,
        mut kill_rx: Option<mpsc::Receiver<()>>,
    ) -> Result<CommandResult, String> {
        let mut command = Command::new(program);
        command.args(args);

        if let Some(dir) = cwd {
            command.current_dir(dir);
        }

        for (key, value) in env {
            command.env(key, value);
        }

        #[cfg(unix)]
        command.process_group(0);

        command.stdout(Stdio::piped());
        command.stderr(Stdio::piped());

        let mut child = command.spawn().map_err(|e| e.to_string())?;

        let stdout = child.stdout.take().expect("stdout missing");
        let stderr = child.stderr.take().expect("stderr missing");

        let pid = child.id();
        info!("Spawned process with PID: {:?}", pid);

        let stdout_lines = Arc::new(Mutex::new(Vec::new()));
        let stderr_lines = Arc::new(Mutex::new(Vec::new()));

        // Stream stdout
        let stdout_lines_clone = stdout_lines.clone();
        let tx_out = output_tx.clone();
        let stdout_task = tokio::spawn(async move {
            let reader = BufReader::new(stdout);
            let mut lines = reader.lines();
            while let Ok(Some(line)) = lines.next_line().await {
                let msg = format!("stdout: {}", line);
                let _ = tx_out.send(msg).await;
                stdout_lines_clone.lock().await.push(line);
            }
        });

        // Stream stderr
        let stderr_lines_clone = stderr_lines.clone();
        let tx_err = output_tx.clone();
        let stderr_task = tokio::spawn(async move {
            let reader = BufReader::new(stderr);
            let mut lines = reader.lines();
            while let Ok(Some(line)) = lines.next_line().await {
                let msg = format!("stderr: {}", line);
                let _ = tx_err.send(msg).await;
                stderr_lines_clone.lock().await.push(line);
            }
        });

        // Wait for process or kill signal
        let wait_result = if let Some(ref mut rx) = kill_rx {
            tokio::select! {
                status = child.wait() => {
                    match status {
                        Ok(s) => Some(s),
                        Err(e) => {
                            error!("Failed to wait for process: {}", e);
                            None
                        }
                    }
                }
                _ = rx.recv() => {
                    info!("Kill signal received, terminating process");
                    #[cfg(unix)]
                    {
                        if let Some(pid) = pid {
                            use nix::sys::signal::{killpg, Signal};
                            use nix::unistd::Pid;
                            let _ = killpg(Pid::from_raw(pid as i32), Signal::SIGINT);
                        }
                    }
                    let _ = child.kill().await;
                    child.wait().await.ok()
                }
            }
        } else {
            child.wait().await.ok()
        };

        // Wait for streaming tasks
        let _ = tokio::join!(stdout_task, stderr_task);

        let success = wait_result.as_ref().map(|s| s.success()).unwrap_or(false);
        let exit_code = wait_result.and_then(|s| s.code());

        Ok(CommandResult {
            success,
            exit_code,
            stdout_lines: stdout_lines.lock().await.clone(),
            stderr_lines: stderr_lines.lock().await.clone(),
        })
    }
}

/// Mock command executor for testing
///
/// This executor doesn't actually spawn processes, but returns pre-configured
/// results. It's useful for testing build logic without requiring actual build tools.
pub struct MockCommandExecutor {
    responses: Arc<Mutex<Vec<MockResponse>>>,
    call_index: Arc<Mutex<usize>>,
}

#[derive(Debug, Clone)]
pub struct MockResponse {
    pub program_pattern: String,
    pub success: bool,
    pub exit_code: i32,
    pub stdout_lines: Vec<String>,
    pub stderr_lines: Vec<String>,
    pub delay_ms: Option<u64>,
}

impl MockCommandExecutor {
    pub fn new() -> Self {
        Self {
            responses: Arc::new(Mutex::new(Vec::new())),
            call_index: Arc::new(Mutex::new(0)),
        }
    }

    /// Add a mock response for a command
    pub async fn add_response(&self, response: MockResponse) {
        let mut responses = self.responses.lock().await;
        responses.push(response);
    }

    /// Add a simple success response
    pub async fn add_success(&self, program_pattern: &str, stdout: Vec<String>) {
        self.add_response(MockResponse {
            program_pattern: program_pattern.to_string(),
            success: true,
            exit_code: 0,
            stdout_lines: stdout,
            stderr_lines: Vec::new(),
            delay_ms: None,
        })
        .await;
    }

    /// Add a simple failure response
    pub async fn add_failure(&self, program_pattern: &str, stderr: Vec<String>) {
        self.add_response(MockResponse {
            program_pattern: program_pattern.to_string(),
            success: false,
            exit_code: 1,
            stdout_lines: Vec::new(),
            stderr_lines: stderr,
            delay_ms: None,
        })
        .await;
    }
}

#[async_trait]
impl CommandExecutor for MockCommandExecutor {
    async fn execute(
        &self,
        program: &str,
        args: &[&str],
        _cwd: Option<PathBuf>,
        _env: &[(String, String)],
        output_tx: mpsc::Sender<String>,
        _kill_rx: Option<mpsc::Receiver<()>>,
    ) -> Result<CommandResult, String> {
        let mut index = self.call_index.lock().await;
        let responses = self.responses.lock().await;

        debug!("Mock executing: {} {:?}", program, args);

        // Find matching response
        let response = if *index < responses.len() {
            responses[*index].clone()
        } else {
            // No more responses configured, return default failure
            return Err(format!("No mock response configured for call #{}", *index));
        };

        *index += 1;
        drop(index);
        drop(responses);

        // Simulate delay if configured
        if let Some(delay) = response.delay_ms {
            tokio::time::sleep(tokio::time::Duration::from_millis(delay)).await;
        }

        // Send stdout lines
        for line in &response.stdout_lines {
            let msg = format!("stdout: {}", line);
            let _ = output_tx.send(msg).await;
        }

        // Send stderr lines
        for line in &response.stderr_lines {
            let msg = format!("stderr: {}", line);
            let _ = output_tx.send(msg).await;
        }

        Ok(CommandResult {
            success: response.success,
            exit_code: Some(response.exit_code),
            stdout_lines: response.stdout_lines.clone(),
            stderr_lines: response.stderr_lines.clone(),
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[tokio::test]
    async fn test_mock_executor_success() {
        let executor = MockCommandExecutor::new();
        executor
            .add_success("git", vec!["Cloning into 'repo'...".to_string()])
            .await;

        let (tx, mut rx) = mpsc::channel(100);
        let result = executor
            .execute(
                "git",
                &["clone", "https://example.com/repo.git"],
                None,
                &[],
                tx,
                None,
            )
            .await;

        assert!(result.is_ok());
        let cmd_result = result.unwrap();
        assert!(cmd_result.success);
        assert_eq!(cmd_result.exit_code, Some(0));
        assert_eq!(cmd_result.stdout_lines.len(), 1);

        // Check that output was sent
        let msg = rx.recv().await;
        assert!(msg.is_some());
        assert!(msg.unwrap().contains("Cloning into 'repo'"));
    }

    #[tokio::test]
    async fn test_mock_executor_failure() {
        let executor = MockCommandExecutor::new();
        executor
            .add_failure("make", vec!["error: missing target".to_string()])
            .await;

        let (tx, _rx) = mpsc::channel(100);
        let result = executor
            .execute("make", &["defconfig"], None, &[], tx, None)
            .await;

        assert!(result.is_ok());
        let cmd_result = result.unwrap();
        assert!(!cmd_result.success);
        assert_eq!(cmd_result.exit_code, Some(1));
        assert_eq!(cmd_result.stderr_lines.len(), 1);
    }
}
