use super::{
    builder_config::BuilderConfig,
    domain::{BuildContext, PerBuildIdStatus},
    kernel_config::KernelConfig,
};
use crate::{
    build_common::{ProcessRunner, RealProcessRunner},
    filesystem::{Filesystem, RealFilesystem},
    git_repo::{GitProvider, RealGitProvider},
};
use std::fs::File;
use std::path::Path;
use std::path::PathBuf;
use std::sync::Arc;
use tokio::sync::{Mutex, broadcast};
use tonic::Status;
use tracing::{debug, info, warn};
use zip::CompressionMethod;
use zip::ZipWriter;
use zip::write::FileOptions; // for .process_group()

pub struct BuildService {
    pub(crate) kernel_configs: Arc<Mutex<Vec<KernelConfig>>>,
    pub(crate) contexts: Arc<Mutex<Vec<BuildContext>>>,
    pub(crate) id: Arc<Mutex<i32>>,
    pub(crate) build_statuses: Arc<Mutex<Vec<PerBuildIdStatus>>>,
    pub(crate) builder_config: BuilderConfig,
    pub(crate) temp_directory: PathBuf,
    pub(crate) output_directory: PathBuf,
    pub shutdown_tx: broadcast::Sender<()>, // Channel to signal shutdown
    // Command-execution seam; defaults to RealProcessRunner, swappable in tests.
    pub(crate) runner: Arc<dyn ProcessRunner>,
    // Git seam; defaults to RealGitProvider, swappable in tests.
    pub(crate) git: Arc<dyn GitProvider>,
    // Filesystem seam; defaults to RealFilesystem, swappable in tests.
    pub(crate) fs: Arc<dyn Filesystem>,
}
pub(crate) type WrappedBuildStatus = Arc<Mutex<Vec<PerBuildIdStatus>>>;
pub(crate) type WrappedContexts = Arc<Mutex<Vec<BuildContext>>>;

impl BuildService {
    pub(crate) fn validate_config_name(name: &str) -> Result<(), Status> {
        if name.is_empty() {
            return Err(Status::invalid_argument("Config name cannot be empty"));
        }

        let path = Path::new(name);
        if path.is_absolute() {
            return Err(Status::invalid_argument(
                "Config name must be a relative path component",
            ));
        }

        if path.components().count() != 1 || path.parent().is_some() {
            return Err(Status::invalid_argument(
                "Config name must be a single path component",
            ));
        }

        if name == "." || name == ".." {
            return Err(Status::invalid_argument("Config name is not allowed"));
        }

        Ok(())
    }

    pub fn new(
        kernel_configs: Vec<KernelConfig>,
        builder_config: BuilderConfig,
        temp_directory: PathBuf,
        output_directory: PathBuf,
    ) -> Self {
        let (shutdown_tx, _) = broadcast::channel(1);

        info!(
            "BuildService initialized with {} kernel configs.",
            &kernel_configs.len()
        );
        info!(
            "Names: {:?}",
            kernel_configs
                .iter()
                .map(|c| &c.name)
                .collect::<Vec<&String>>()
        );
        let contexts: Arc<Mutex<Vec<BuildContext>>> = Arc::new(Mutex::new(Vec::new()));
        let contexts_clone = contexts.clone();
        let shutdown_on_signal = shutdown_tx.clone();

        tokio::spawn(async move {
            tokio::signal::ctrl_c()
                .await
                .expect("Failed to listen for Ctrl-C");
            info!("Global Ctrl-C received");

            // Cancel any active build so its subprocess gets SIGINT and cleans up.
            let job = contexts_clone.lock().await;
            if let Some(active) = job.iter().find(|c| c.kill_signal.is_some()) {
                info!(
                    "Active build found (ID: {}), sending kill signal...",
                    active.id
                );
                if let Some(kill_tx) = &active.kill_signal {
                    let _ = kill_tx.send(()).await;
                    info!("Kill signal sent to active build (ID: {})", active.id);
                } else {
                    warn!(
                        "No kill signal channel found for active build (ID: {})",
                        active.id
                    );
                }
            } else {
                info!("No active build found, proceeding with shutdown.");
            }

            // Request a graceful shutdown rather than std::process::exit, so main
            // returns and its TempDir Drop removes the temporary working directory.
            info!("Requesting graceful shutdown");
            let _ = shutdown_on_signal.send(());
        });

        BuildService {
            kernel_configs: Arc::new(Mutex::new(kernel_configs)),
            contexts: contexts,
            id: Arc::new(Mutex::new(0)),
            build_statuses: Arc::new(Mutex::new(Vec::new())),
            builder_config,
            temp_directory,
            output_directory,
            shutdown_tx,
            runner: Arc::new(RealProcessRunner),
            git: Arc::new(RealGitProvider),
            fs: Arc::new(RealFilesystem),
        }
    }

    pub(crate) async fn add_artifact_path_to_context(
        contexts: &WrappedContexts,
        build_id: i32,
        archive_file_path: &Path,
    ) {
        let mut ctxs = contexts.lock().await;
        if let Some(ctx) = ctxs.iter_mut().find(|c| c.id == build_id) {
            ctx.artifact_path = Some(archive_file_path.to_path_buf());
        }
    }

    pub(crate) async fn mark_build_finished(
        peridstat: &WrappedBuildStatus,
        build_id: i32,
        success: bool,
    ) {
        let mut statuses = peridstat.lock().await;
        if let Some(entry) = statuses.iter_mut().find(|s| s.build_id == build_id) {
            entry.finished = true;
            entry.succeeded = success;
        }
    }

    pub(crate) async fn is_build_finished(peridstat: &WrappedBuildStatus, build_id: i32) -> bool {
        let statuses = peridstat.lock().await;
        if let Some(entry) = statuses.iter().find(|s| s.build_id == build_id) {
            entry.finished
        } else {
            false
        }
    }

    pub(crate) async fn is_valid_build_id(peridstat: &WrappedBuildStatus, build_id: i32) -> bool {
        let statuses = peridstat.lock().await;
        statuses.iter().any(|s| s.build_id == build_id)
    }

    pub(crate) async fn inc_and_get_build_id(id_lock: &Arc<Mutex<i32>>) -> i32 {
        let mut id_guard = id_lock.lock().await;
        *id_guard += 1;
        *id_guard
    }

    pub(crate) async fn zip_dir_with_filename(
        archive_file_path: &Path,
        target_dir: &Path,
    ) -> Result<(), Status> {
        let file = File::create(&archive_file_path)?;
        let mut zip = ZipWriter::new(file);

        let options = FileOptions::<()>::default().compression_method(CompressionMethod::Deflated);

        for entry in walkdir::WalkDir::new(&target_dir) {
            let entry = entry.map_err(|e| {
                Status::internal(format!(
                    "Failed to read entry in target directory {:?}: {}",
                    target_dir, e
                ))
            })?;
            let path = entry.path();
            if path.is_file()
                && !path
                    .file_name()
                    .unwrap_or_default()
                    .to_string_lossy()
                    .starts_with(".")
            {
                let name = path
                    .strip_prefix(&target_dir)
                    .map_err(|e| {
                        Status::internal(format!(
                            "Failed to get relative path for {:?}: {}",
                            path, e
                        ))
                    })?
                    .to_string_lossy()
                    .to_string();
                if std::fs::File::open(&path)?
                    .metadata()
                    .map_err(|e| {
                        Status::internal(format!(
                            "Failed to get metadata for file {:?}: {}",
                            path, e
                        ))
                    })?
                    .len()
                    == 0
                {
                    debug!(
                        "Skipping zero-length file in target directory zip: {:?}",
                        path
                    );
                    continue;
                }
                zip.start_file(name, options).map_err(|e| {
                    Status::internal(format!("Failed to add file to zip {:?}: {}", path, e))
                })?;
                let mut f = std::fs::File::open(&path).map_err(|e| {
                    Status::internal(format!("Failed to open file {:?} for zipping: {}", path, e))
                })?;
                std::io::copy(&mut f, &mut zip).map_err(|e| {
                    Status::internal(format!("Failed to copy file {:?} into zip: {}", path, e))
                })?;
                debug!("Added file to target directory zip: {:?}", path);
            }
        }
        zip.finish()
            .map_err(|e| Status::internal(format!("Failed to finalize zip file: {}", e)))?;
        Ok(())
    }
}

#[cfg(test)]
mod runner_tests {
    use super::*;
    use crate::build_common::{BuildError, BuildEventSender, CommandOutcome, CommandSpec};
    use crate::kernelbuild::harness::run_process;
    use async_trait::async_trait;
    use std::collections::VecDeque;
    use tokio::{process::Command, sync::mpsc};

    /// Records each command's argv and returns scripted results instead of
    /// spawning a process, so the build workflow can be exercised without real
    /// `make` invocations.
    #[derive(Default)]
    pub(crate) struct MockProcessRunner {
        pub calls: Mutex<Vec<Vec<String>>>,
        pub results: Mutex<VecDeque<bool>>,
    }

    impl MockProcessRunner {
        fn argv(command: &Command) -> Vec<String> {
            let std = command.as_std();
            std::iter::once(std.get_program())
                .chain(std.get_args())
                .map(|s| s.to_string_lossy().into_owned())
                .collect()
        }
    }

    #[async_trait]
    impl ProcessRunner for MockProcessRunner {
        async fn run(
            &self,
            spec: CommandSpec,
            _events: Arc<dyn BuildEventSender>,
            _kill_rx: Option<&mut mpsc::Receiver<()>>,
        ) -> Result<CommandOutcome, BuildError> {
            self.calls.lock().await.push(Self::argv(&spec.command));
            let success = self.results.lock().await.pop_front().unwrap_or(true);
            Ok(CommandOutcome {
                success,
                exit_code: success.then_some(0),
                cancelled: false,
            })
        }
    }

    #[tokio::test]
    async fn mock_records_argv_and_returns_scripted_result() {
        let runner = MockProcessRunner::default();
        runner.results.lock().await.push_back(false);

        let (tx, _rx) = mpsc::channel(16);
        let mut cmd = Command::new("make");
        cmd.arg("defconfig");

        let ok = run_process(&runner, cmd, tx, Some(7), None, None)
            .await
            .unwrap();

        assert!(!ok, "scripted failure should propagate");
        let calls = runner.calls.lock().await;
        assert_eq!(calls.len(), 1);
        assert_eq!(calls[0], vec!["make", "defconfig"]);
    }

    #[tokio::test]
    async fn injected_runner_is_a_trait_object() {
        // The struct field is Arc<dyn ProcessRunner>; confirm the mock satisfies it.
        let runner: Arc<dyn ProcessRunner> = Arc::new(MockProcessRunner::default());
        let (tx, _rx) = mpsc::channel(16);
        let ok = run_process(runner.as_ref(), Command::new("true"), tx, None, None, None)
            .await
            .unwrap();
        assert!(ok, "default scripted result is success");
    }
}
