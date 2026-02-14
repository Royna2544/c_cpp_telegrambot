/// Example tests showing how to use MockCommandExecutor
///
/// This module demonstrates the testing pattern without requiring
/// full integration with the build services (which need protoc to compile).

#[cfg(test)]
mod tests {
    use crate::command_executor::{CommandExecutor, MockCommandExecutor, MockResponse};
    use tokio::sync::mpsc;

    #[tokio::test]
    async fn test_git_clone_success() {
        // Setup mock executor with expected git clone behavior
        let executor = MockCommandExecutor::new();
        executor
            .add_response(MockResponse {
                program_pattern: "git".to_string(),
                success: true,
                exit_code: 0,
                stdout_lines: vec![
                    "Cloning into 'linux'...".to_string(),
                    "remote: Enumerating objects: 1000".to_string(),
                    "Receiving objects: 100% (1000/1000)".to_string(),
                ],
                stderr_lines: vec![],
                delay_ms: Some(100), // Simulate 100ms clone time
            })
            .await;

        // Execute the command
        let (tx, mut rx) = mpsc::channel(100);
        let result = executor
            .execute(
                "git",
                &[
                    "clone",
                    "--depth=1",
                    "https://github.com/torvalds/linux.git",
                ],
                None,
                &[],
                tx,
                None,
            )
            .await;

        // Verify result
        assert!(result.is_ok());
        let cmd_result = result.unwrap();
        assert!(cmd_result.success);
        assert_eq!(cmd_result.exit_code, Some(0));
        assert_eq!(cmd_result.stdout_lines.len(), 3);

        // Verify output was streamed
        let mut output_count = 0;
        while let Ok(msg) = rx.try_recv() {
            assert!(msg.starts_with("stdout: "));
            output_count += 1;
        }
        assert_eq!(output_count, 3);
    }

    #[tokio::test]
    async fn test_make_defconfig_success() {
        let executor = MockCommandExecutor::new();
        executor
            .add_success(
                "make",
                vec![
                    "HOSTCC  scripts/basic/fixdep".to_string(),
                    "HOSTCC  scripts/kconfig/conf.o".to_string(),
                    "configuration written to .config".to_string(),
                ],
            )
            .await;

        let (tx, _rx) = mpsc::channel(100);
        let result = executor
            .execute(
                "make",
                &["-j8", "O=out", "defconfig"],
                None,
                &[("ARCH".to_string(), "arm64".to_string())],
                tx,
                None,
            )
            .await;

        assert!(result.is_ok());
        let cmd_result = result.unwrap();
        assert!(cmd_result.success);
        assert_eq!(cmd_result.stdout_lines.len(), 3);
    }

    #[tokio::test]
    async fn test_make_build_failure() {
        let executor = MockCommandExecutor::new();
        executor
            .add_failure(
                "make",
                vec![
                    "error: implicit declaration of function 'foo'".to_string(),
                    "make[1]: *** [scripts/Makefile.build:271: file.o] Error 1".to_string(),
                    "make: *** [Makefile:1872: file] Error 2".to_string(),
                ],
            )
            .await;

        let (tx, mut rx) = mpsc::channel(100);
        let result = executor
            .execute("make", &["-j8", "O=out"], None, &[], tx, None)
            .await;

        assert!(result.is_ok());
        let cmd_result = result.unwrap();
        assert!(!cmd_result.success); // Build failed
        assert_eq!(cmd_result.exit_code, Some(1));
        assert_eq!(cmd_result.stderr_lines.len(), 3);

        // Verify error output was streamed
        let mut error_count = 0;
        while let Ok(msg) = rx.try_recv() {
            assert!(msg.starts_with("stderr: "));
            error_count += 1;
        }
        assert_eq!(error_count, 3);
    }

    #[tokio::test]
    async fn test_repo_sync_sequence() {
        // Simulate a multi-command build sequence
        let executor = MockCommandExecutor::new();

        // Add responses for multiple commands in sequence
        executor
            .add_success("repo", vec!["Repo command succeeded.".to_string()])
            .await;

        executor
            .add_success(
                "repo",
                vec![
                    "Fetching: 100% (100/100)".to_string(),
                    "Syncing work tree: 100% (100/100)".to_string(),
                ],
            )
            .await;

        executor
            .add_success("bash", vec!["Build completed successfully".to_string()])
            .await;

        // Execute repo init
        let (tx1, _rx1) = mpsc::channel(100);
        let result1 = executor
            .execute("repo", &["init"], None, &[], tx1, None)
            .await;
        assert!(result1.unwrap().success);

        // Execute repo sync
        let (tx2, _rx2) = mpsc::channel(100);
        let result2 = executor
            .execute("repo", &["sync"], None, &[], tx2, None)
            .await;
        assert!(result2.unwrap().success);

        // Execute build script
        let (tx3, _rx3) = mpsc::channel(100);
        let result3 = executor
            .execute("bash", &["build.sh"], None, &[], tx3, None)
            .await;
        assert!(result3.unwrap().success);
    }

    #[tokio::test]
    async fn test_insufficient_responses() {
        // Test what happens when we run out of configured responses
        let executor = MockCommandExecutor::new();
        executor
            .add_success("git", vec!["Success".to_string()])
            .await;

        // First call should succeed
        let (tx1, _rx1) = mpsc::channel(100);
        let result1 = executor
            .execute("git", &["clone"], None, &[], tx1, None)
            .await;
        assert!(result1.is_ok());

        // Second call should fail (no response configured)
        let (tx2, _rx2) = mpsc::channel(100);
        let result2 = executor
            .execute("git", &["clone"], None, &[], tx2, None)
            .await;
        assert!(result2.is_err());
        assert!(result2.unwrap_err().contains("No mock response"));
    }

    /// Example: Simulating a build timeout/cancellation
    #[tokio::test]
    async fn test_build_with_cancellation() {
        let executor = MockCommandExecutor::new();
        executor
            .add_response(MockResponse {
                program_pattern: "make".to_string(),
                success: false,
                exit_code: -1, // Typically indicates killed/interrupted
                stdout_lines: vec!["Building...".to_string()],
                stderr_lines: vec![],
                delay_ms: Some(5000), // Would take 5 seconds
            })
            .await;

        let (tx, _rx) = mpsc::channel(100);
        let (kill_tx, kill_rx) = mpsc::channel(1);

        // Spawn the execution
        let exec_handle = tokio::spawn(async move {
            executor
                .execute("make", &[], None, &[], tx, Some(kill_rx))
                .await
        });

        // Simulate cancellation after 100ms
        tokio::time::sleep(tokio::time::Duration::from_millis(100)).await;
        let _ = kill_tx.send(()).await;

        // For MockCommandExecutor, kill signal is currently ignored
        // In a real implementation, this would terminate the process
        let result = exec_handle.await.unwrap();
        assert!(result.is_ok());
    }
}
