# Testable Build Services - Integration Guide

## Overview

This guide explains how to integrate the `CommandExecutor` trait into the build services to make them testable without spawning actual processes.

## Architecture

### CommandExecutor Trait

The `CommandExecutor` trait provides an abstraction for executing external commands:

```rust
#[async_trait]
pub trait CommandExecutor: Send + Sync {
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
```

### Implementations

1. **RealCommandExecutor**: Production implementation that spawns actual processes
2. **MockCommandExecutor**: Test implementation that returns pre-configured responses

## Integration Steps

### Step 1: Make BuildService Generic

Modify the `BuildService` struct to be generic over the command executor:

```rust
pub struct BuildService<E: CommandExecutor> {
    kernel_configs: Arc<Mutex<Vec<KernelConfig>>>,
    contexts: Arc<Mutex<Vec<BuildContext>>>,
    id: Arc<Mutex<i32>>,
    build_statuses: Arc<Mutex<Vec<PerBuildIdStatus>>>,
    builder_config: BuilderConfig,
    temp_directory: PathBuf,
    output_directory: PathBuf,
    executor: Arc<E>,  // Add this field
}
```

### Step 2: Update Constructor

Add the executor to the constructor:

```rust
impl<E: CommandExecutor> BuildService<E> {
    pub fn new(
        kernel_configs: Vec<KernelConfig>,
        builder_config: BuilderConfig,
        temp_directory: PathBuf,
        output_directory: PathBuf,
        executor: Arc<E>,
    ) -> Self {
        Self {
            kernel_configs: Arc::new(Mutex::new(kernel_configs)),
            contexts: Arc::new(Mutex::new(Vec::new())),
            id: Arc::new(Mutex::new(0)),
            build_statuses: Arc::new(Mutex::new(Vec::new())),
            builder_config,
            temp_directory,
            output_directory,
            executor,
        }
    }
}
```

### Step 3: Replace Direct Command Usage

Find all places where `Command::new()` is used and replace with executor calls.

**Before:**
```rust
let mut proc = Command::new("make");
proc.current_dir(&source_dir)
    .arg(format!("-j{}", available_parallelism().unwrap().get()))
    .arg("O=out")
    .arg(&defconfig_name);

let success = BuildService::run_command_with_logs(
    proc, tx.clone(), None, None, Some(log_file)
).await?;
```

**After:**
```rust
let (output_tx, output_rx) = mpsc::channel(1000);

// Spawn task to convert output format
let tx_clone = tx.clone();
tokio::spawn(async move {
    while let Some(line) = output_rx.recv().await {
        // Convert "stdout: line" or "stderr: line" to BuildStatus
        // and send via tx_clone
    }
});

let result = self.executor.execute(
    "make",
    &[
        &format!("-j{}", available_parallelism().unwrap().get()),
        "O=out",
        &defconfig_name,
    ],
    Some(source_dir.clone()),
    &env_vars,
    output_tx,
    kill_rx,
).await?;

let success = result.success;
```

### Step 4: Update main.rs

In production code, use the real executor:

```rust
use builder::command_executor::RealCommandExecutor;
use std::sync::Arc;

let executor = Arc::new(RealCommandExecutor::new());
let kernel_build_service = BuildService::new(
    kernel_configs,
    builder_config,
    temp_dir.clone(),
    output_dir.clone(),
    executor.clone(),
);
```

### Step 5: Write Tests

Create tests using the mock executor:

```rust
#[cfg(test)]
mod tests {
    use super::*;
    use crate::command_executor::{MockCommandExecutor, MockResponse};
    use std::sync::Arc;

    #[tokio::test]
    async fn test_successful_build() {
        // Setup mock executor
        let executor = Arc::new(MockCommandExecutor::new());
        
        // Configure expected command responses
        executor.add_success(
            "git",
            vec!["Cloning into 'linux'...".to_string()]
        );
        executor.add_success(
            "make",
            vec!["configuration written to .config".to_string()]
        );
        executor.add_success(
            "make",
            vec!["Kernel: arch/arm64/boot/Image is ready".to_string()]
        );

        // Create build service with mock executor
        let build_service = BuildService::new(
            test_configs,
            test_builder_config,
            temp_dir.clone(),
            output_dir.clone(),
            executor.clone(),
        );

        // Test the build flow
        // ... make gRPC calls to build service ...
        
        // Verify results without spawning real processes
    }

    #[tokio::test]
    async fn test_build_failure() {
        let executor = Arc::new(MockCommandExecutor::new());
        
        // Configure to simulate a build error
        executor.add_failure(
            "make",
            vec!["error: implicit declaration".to_string()]
        );

        // Test that failure is handled correctly
        // ...
    }
}
```

## Benefits

1. **Fast Tests**: No actual process spawning, tests run in milliseconds
2. **Deterministic**: Pre-configured responses ensure repeatable test results
3. **Isolated**: Tests don't require build tools (make, gcc, etc.) to be installed
4. **Comprehensive**: Can test error scenarios that are hard to reproduce with real processes
5. **CI Friendly**: Tests run in any environment without dependencies

## Migration Strategy

1. Start with `kernelbuild/build_service.rs`
2. Add executor parameter to struct and constructor
3. Identify all `Command::new()` calls
4. Replace with `executor.execute()` calls
5. Add tests for one feature at a time
6. Repeat for `rombuild/build_service.rs`
7. Update `main.rs` to inject `RealCommandExecutor`

## Testing Tips

- Use `add_success()` for simple successful commands
- Use `add_failure()` for error scenarios
- Use `MockResponse` with `delay_ms` to simulate long-running builds
- Test command sequences by adding multiple responses
- Test cancellation by using the kill_rx channel
- Verify output streaming by checking the output_tx channel

## Example Test Scenarios

1. **Successful kernel build**:
   - git clone → make defconfig → make → artifact creation
   
2. **Build failure**:
   - git clone → make defconfig → make fails → error log capture
   
3. **Cancellation**:
   - Start build → send kill signal → verify cleanup
   
4. **Configuration update**:
   - Verify config changes are applied correctly
   
5. **Resource cleanup**:
   - Verify temp directories are cleaned up after build

## Current Status

- ✅ CommandExecutor trait created
- ✅ RealCommandExecutor implemented
- ✅ MockCommandExecutor implemented
- ✅ Basic unit tests for executor
- ⏳ Integration into BuildService (next step)
- ⏳ Comprehensive build service tests (next step)
