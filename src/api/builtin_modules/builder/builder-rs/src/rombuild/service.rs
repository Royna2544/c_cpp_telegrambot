use crate::{
    build_common::{BuildArtifact, BuildError, ProcessRunner, RealProcessRunner},
    filesystem::{Filesystem, RealFilesystem},
    git_repo::{GitProvider, RealGitProvider},
    rombuild::{
        domain::{RomBuildSettings, RomBuildTask, RomBuildVariant, RomUploadMethod},
        grpc::grpc_pb::BuildLogEntry,
        registry::RomBuildRegistry,
        types::{ROMArtifactMatcher, ROMBuildConfig, ROMEntry, TargetsEntry},
    },
};

#[cfg(unix)]
use nix::libc::{rlimit, setrlimit};
use std::{
    path::{Path, PathBuf},
    sync::Arc,
};
use tokio::{
    process::Command,
    sync::{Mutex, broadcast, mpsc},
    task::JoinHandle,
};
use tonic::Status;
use tracing::info;

pub(crate) struct ActiveBuild {
    pub(crate) id: String,
    // Used to send "Stop!" signal
    pub(crate) kill_tx: mpsc::Sender<()>,
    // Used to broadcast logs to any connected client
    pub(crate) log_tx: broadcast::Sender<BuildLogEntry>,
    // Used to wait for the task to finish (optional, for cleanup)
    pub(crate) _task: JoinHandle<Result<(), BuildError>>,
}

#[derive(Clone)]
pub(crate) struct UploadTask {
    pub(crate) method: RomUploadMethod,
    pub(crate) build_id: String,
    pub(crate) artifact: BuildArtifact,
}

pub(crate) enum BuildStatus {
    InProgress,
    Success,
    Failed(String), // Include error message if failed
}

pub(crate) struct BuildEntry {
    pub(crate) id: String,
    pub(crate) variant: RomBuildVariant,
    pub(crate) target_device: TargetsEntry,
    pub(crate) config_name: String,
    pub(crate) success: BuildStatus,
}

/// Owned state handed to the ROM build harness: everything the build
/// pipeline needs, captured up front so the task is `'static` and the pipeline
/// is directly awaitable in tests (inject mock runner/git/fs, drive failures).
pub(crate) struct BuildTask {
    pub(crate) domain: RomBuildTask,
    pub(crate) log_tx_clone: broadcast::Sender<BuildLogEntry>,
    pub(crate) registry: Arc<RomBuildRegistry>,
    pub(crate) askpass_path_clone: Option<PathBuf>,
    pub(crate) runner: Arc<dyn ProcessRunner>,
    pub(crate) git: Arc<dyn GitProvider>,
    pub(crate) fs: Arc<dyn Filesystem>,
    pub(crate) kill_rx: mpsc::Receiver<()>,
    pub(crate) span: tracing::Span,
}

pub struct BuildService {
    pub(crate) settings: Arc<Mutex<RomBuildSettings>>,
    pub(crate) build_dir: PathBuf,
    pub(crate) tempdir: PathBuf,
    pub(crate) configs: ROMBuildConfig,
    pub(crate) registry: Arc<RomBuildRegistry>,
    pub shutdown_tx: broadcast::Sender<()>, // Channel to signal shutdown
    // Command-execution seam; defaults to RealProcessRunner, swappable in tests.
    pub(crate) runner: Arc<dyn ProcessRunner>,
    // Git seam; defaults to RealGitProvider, swappable in tests.
    pub(crate) git: Arc<dyn GitProvider>,
    // Filesystem seam; defaults to RealFilesystem, swappable in tests.
    pub(crate) fs: Arc<dyn Filesystem>,
}

impl BuildService {
    pub(crate) fn shell_single_quote(value: &str) -> String {
        format!("'{}'", value.replace('\'', "'\\''"))
    }

    pub(crate) fn configure_repo_command_env(
        command: &mut Command,
        build_dir: &Path,
        askpass_path: Option<&Path>,
    ) {
        command.env("REPO_CONFIG_DIR", build_dir);
        if let Some(path) = askpass_path {
            command.env("GIT_ASKPASS", path);
        }
    }

    fn is_safe_relative_path(path: &Path) -> bool {
        path.components()
            .all(|component| matches!(component, std::path::Component::Normal(_)))
    }

    pub(crate) async fn setup_rbe_env(
        fs: &dyn Filesystem,
        build_dir: PathBuf,
        rbe_api_token: &str,
    ) -> Result<(), Status> {
        let rbe_env_path = build_dir.join("rbe_env.sh");
        let rbe_cli_path = build_dir.join("rbe_cli");

        const RBE_CLI_URL: &str =
            "https://chrome-infra-packages.appspot.com/dl/infra/rbe/client/linux-amd64/+/stable";
        if !fs.exists(&rbe_cli_path) {
            // Download rbe_cli binary
            info!("Downloading RBE CLI from {}", RBE_CLI_URL);
            let client = reqwest::Client::new();
            let response = client
                .get(RBE_CLI_URL)
                .send()
                .await
                .map_err(|e| Status::internal(format!("Failed to download RBE CLI: {}", e)))?;

            if !response.status().is_success() {
                return Err(Status::internal(format!(
                    "Failed to download RBE CLI: HTTP {}",
                    response.status()
                )));
            }
            let bytes = response
                .bytes()
                .await
                .map_err(|e| Status::internal(format!("Failed to read RBE CLI response: {}", e)))?;
            // Unzip the downloaded file.
            let mut archive = zip::ZipArchive::new(std::io::Cursor::new(bytes))
                .map_err(|e| Status::internal(format!("Failed to read RBE CLI zip: {}", e)))?;
            // Unzip the files to rbe_cli/
            archive
                .extract(&rbe_cli_path)
                .map_err(|e| Status::internal(format!("Failed to extract RBE CLI: {}", e)))?;
        }
        let content = format!(
            r##"# Remote Build Execution (RBE) configurations
# See https://nopenopeguy.github.io/rbe for more information

# --- Enable RBE and General Settings ---
export USE_RBE=1
export RBE_DIR="{}"
export NINJA_REMOTE_NUM_JOBS=128

# --- BuildBuddy Connection Settings ---
export RBE_service="aosp.buildbuddy.io:443"
export RBE_remote_headers={}
export RBE_use_rpc_credentials=false
export RBE_service_no_auth=true

# --- Unified Downloads/Uploads (Recommended) ---
export RBE_use_unified_downloads=true
export RBE_use_unified_uploads=true

# --- Execution Strategies (remote_local_fallback is generally best) ---
export RBE_R8_EXEC_STRATEGY=remote_local_fallback
export RBE_D8_EXEC_STRATEGY=remote_local_fallback
export RBE_JAVAC_EXEC_STRATEGY=remote_local_fallback
export RBE_JAR_EXEC_STRATEGY=remote_local_fallback
export RBE_ZIP_EXEC_STRATEGY=remote_local_fallback
export RBE_TURBINE_EXEC_STRATEGY=remote_local_fallback
export RBE_SIGNAPK_EXEC_STRATEGY=remote_local_fallback
export RBE_CXX_EXEC_STRATEGY=remote_local_fallback
export RBE_CXX_LINKS_EXEC_STRATEGY=remote_local_fallback
export RBE_ABI_LINKER_EXEC_STRATEGY=remote_local_fallback
export RBE_CLANG_TIDY_EXEC_STRATEGY=remote_local_fallback
export RBE_METALAVA_EXEC_STRATEGY=remote_local_fallback
export RBE_LINT_EXEC_STRATEGY=remote_local_fallback

# --- Enable RBE for Specific Tools ---
export RBE_R8=1
export RBE_D8=1
export RBE_JAVAC=1
export RBE_JAR=1
export RBE_ZIP=1
export RBE_TURBINE=1
export RBE_SIGNAPK=1
export RBE_CXX_LINKS=1
export RBE_CXX=1
export RBE_ABI_LINKER=1
export RBE_CLANG_TIDY=1
export RBE_METALAVA=1
export RBE_LINT=1

# --- Resource Pools ---
export RBE_JAVA_POOL=default
export RBE_METALAVA_POOL=default
export RBE_LINT_POOL=default"##,
            Self::shell_single_quote(&rbe_cli_path.to_string_lossy()),
            Self::shell_single_quote(&format!("x-buildbuddy-api-key={}", rbe_api_token))
        );
        fs.write(&rbe_env_path, content.as_bytes())
            .map_err(|e| Status::internal(format!("Failed to write RBE env file: {}", e)))?;
        #[cfg(unix)]
        {
            fs.set_mode(&rbe_env_path, 0o600).map_err(|e| {
                Status::internal(format!("Failed to set permissions for RBE env file: {}", e))
            })?;
        }
        Ok(())
    }

    pub fn new(build_dir: PathBuf, temp_dir: PathBuf, configs: ROMBuildConfig) -> Self {
        let (shutdown_tx, _) = broadcast::channel(1);

        let registry = Arc::new(RomBuildRegistry::new());
        let active_job_clone = registry.active.clone();
        let shutdown_on_signal = shutdown_tx.clone();

        tokio::spawn(async move {
            tokio::signal::ctrl_c()
                .await
                .expect("Failed to listen for Ctrl-C");
            info!("Global Ctrl-C received");

            // Cancel any active build so its subprocess gets SIGINT and cleans up.
            let job = active_job_clone.lock().await;
            if let Some(build) = job.as_ref() {
                info!("Cancelling active build: {}", build.id);
                let _ = build.kill_tx.send(()).await;
                drop(job); // Release lock

                // Wait a bit for cleanup
                tokio::time::sleep(tokio::time::Duration::from_secs(5)).await;
            }

            // Request a graceful shutdown rather than std::process::exit, so main
            // returns and its TempDir Drop removes the temporary working directory.
            info!("Requesting graceful shutdown");
            let _ = shutdown_on_signal.send(());
        });

        BuildService {
            settings: Arc::new(Mutex::new(RomBuildSettings::default())),
            build_dir,
            tempdir: temp_dir,
            configs,
            registry,
            shutdown_tx,
            runner: Arc::new(RealProcessRunner),
            git: Arc::new(RealGitProvider),
            fs: Arc::new(RealFilesystem),
        }
    }
}

impl BuildService {
    pub(crate) async fn find_artifact(
        fs: &dyn Filesystem,
        build_dir: &PathBuf,
        codename: &String,
        rom_entry: &ROMEntry,
    ) -> Result<Vec<PathBuf>, Status> {
        let output_dir = build_dir
            .join("out")
            .join("target")
            .join("product")
            .join(&codename);
        match rom_entry.artifact.matcher {
            ROMArtifactMatcher::ZipFilePrefixer => {
                let prefix = &rom_entry.artifact.data;
                let mut artifact_paths: Vec<PathBuf> = Vec::new();
                let mut found = false;
                for entry in fs.read_dir(&output_dir).map_err(|e| {
                    tonic::Status::internal(format!("Failed to read output directory: {}", e))
                })? {
                    let file_name = entry
                        .file_name()
                        .unwrap_or_default()
                        .to_string_lossy()
                        .to_string();
                    if file_name.starts_with(prefix) && file_name.ends_with(".zip") {
                        artifact_paths.push(output_dir.join(&file_name));
                        found = true;
                    }
                }
                if !found {
                    Err(tonic::Status::not_found(
                        "Artifact not found with specified prefix",
                    ))
                } else {
                    Ok(artifact_paths)
                }
            }
            ROMArtifactMatcher::ExactMatcher => {
                let exact_name = &rom_entry.artifact.data;
                let exact_path = Path::new(exact_name);
                if !Self::is_safe_relative_path(exact_path) {
                    return Err(tonic::Status::invalid_argument(format!(
                        "Artifact path must be relative and stay under the product output directory: {}",
                        exact_name
                    )));
                }
                let artifact_path = output_dir.join(exact_path);
                if !fs.is_file(&artifact_path) {
                    Err(tonic::Status::not_found(
                        "Artifact not found with exact name",
                    ))
                } else {
                    Ok(vec![artifact_path])
                }
            }
        }
    }
}

#[cfg(test)]
impl BuildService {
    /// Construct a service around an arbitrary [`ProcessRunner`] for tests,
    /// skipping `new()`'s global Ctrl-C handler. This is the injection point
    /// that lets the build workflow be driven with a mock instead of `repo`/
    /// `bash`.
    pub(crate) fn for_test(
        build_dir: PathBuf,
        temp_dir: PathBuf,
        configs: ROMBuildConfig,
        runner: Arc<dyn ProcessRunner>,
        git: Arc<dyn GitProvider>,
        fs: Arc<dyn Filesystem>,
    ) -> Self {
        let (shutdown_tx, _) = broadcast::channel(1);
        BuildService {
            settings: Arc::new(Mutex::new(RomBuildSettings::default())),
            build_dir,
            tempdir: temp_dir,
            configs,
            registry: Arc::new(RomBuildRegistry::new()),
            shutdown_tx,
            runner,
            git,
            fs,
        }
    }
}

#[cfg(test)]
mod runner_tests {
    use super::*;
    use crate::build_common::{
        BuildError, BuildEvent, BuildEventSender, CommandOutcome, CommandSpec,
    };
    use async_trait::async_trait;
    use std::collections::VecDeque;

    /// Records each command's argv and returns scripted results instead of
    /// spawning a process, so the build workflow can be exercised without real
    /// `repo`/`bash` invocations.
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
            mut spec: CommandSpec,
            _events: Arc<dyn BuildEventSender>,
            _kill_rx: Option<&mut mpsc::Receiver<()>>,
        ) -> Result<CommandOutcome, BuildError> {
            self.calls.lock().await.push(Self::argv(&spec.command));
            // Drain queued stdin so the workflow's senders don't block/observe a
            // closed channel.
            if let Some(rx) = spec.stdin_rx.as_mut() {
                while rx.try_recv().is_ok() {}
            }
            let success = self.results.lock().await.pop_front().unwrap_or(true);
            Ok(CommandOutcome {
                success,
                exit_code: success.then_some(0),
                cancelled: false,
            })
        }

        fn program_available(&self, _program: &str) -> bool {
            true // tests don't need the real `repo`/`bash` binaries installed
        }
    }

    struct NoopEvents;

    #[async_trait]
    impl BuildEventSender for NoopEvents {
        async fn send(&self, _event: BuildEvent) {}
    }

    #[tokio::test]
    async fn mock_records_argv_and_returns_scripted_result() {
        let runner = MockProcessRunner::default();
        runner.results.lock().await.push_back(false);

        let mut cmd = Command::new("repo");
        cmd.arg("sync").arg("-j8");

        let outcome = runner
            .run(CommandSpec::new(cmd), Arc::new(NoopEvents), None)
            .await
            .unwrap();

        assert!(!outcome.success, "scripted failure should propagate");
        let calls = runner.calls.lock().await;
        assert_eq!(calls.len(), 1);
        assert_eq!(calls[0], vec!["repo", "sync", "-j8"]);
    }

    #[tokio::test]
    async fn build_service_accepts_injected_seams() {
        // The seams are injectable: a BuildService can be built around mock
        // command and git providers (the production type defaults to the real
        // ones).
        let runner = Arc::new(MockProcessRunner::default());
        let git = Arc::new(crate::git_repo::mock::MockGitProvider::default());
        let fs = Arc::new(crate::filesystem::mock::MockFilesystem::default());
        let configs = ROMBuildConfig {
            roms: vec![],
            recoveries: vec![],
            targets: vec![],
            manifests: vec![],
            recovery_manifests: vec![],
        };
        let _svc = BuildService::for_test(
            PathBuf::from("/tmp/does-not-exist"),
            PathBuf::from("/tmp/does-not-exist"),
            configs,
            runner.clone(),
            git.clone(),
            fs.clone(),
        );
        assert_eq!(runner.calls.lock().await.len(), 0);
        assert_eq!(git.opens.lock().unwrap().len(), 0);
        assert_eq!(fs.written.lock().unwrap().len(), 0);
    }
}

#[cfg(test)]
mod artifact_path_tests {
    //! Failure-path coverage for find_artifact, driven entirely by the
    //! filesystem mock: "if the artifact lookup / dir read fails, return the
    //! right error and stop".
    use super::*;
    use crate::filesystem::mock::MockFilesystem;
    use crate::rombuild::types::ROMArtifactEntry;

    fn rom_entry(matcher: ROMArtifactMatcher, data: &str) -> ROMEntry {
        ROMEntry {
            name: "rom".into(),
            link: String::new(),
            target: "bacon".into(),
            artifact: ROMArtifactEntry {
                matcher,
                data: data.into(),
            },
            branches: vec![],
        }
    }

    fn product_dir() -> (PathBuf, PathBuf, String) {
        let build_dir = PathBuf::from("/b");
        let codename = "bacon".to_string();
        let out = build_dir
            .join("out")
            .join("target")
            .join("product")
            .join(&codename);
        (build_dir, out, codename)
    }

    #[tokio::test]
    async fn errors_internal_when_output_dir_read_fails() {
        let (build_dir, out, codename) = product_dir();
        let fs = MockFilesystem::default();
        fs.fail_on(out); // read_dir(output_dir) returns an io error
        let entry = rom_entry(ROMArtifactMatcher::ZipFilePrefixer, "lineage-");

        let err = BuildService::find_artifact(&fs, &build_dir, &codename, &entry)
            .await
            .expect_err("a read failure must surface as an error, not an artifact");
        assert_eq!(err.code(), tonic::Code::Internal);
    }

    #[tokio::test]
    async fn not_found_when_no_zip_matches_prefix() {
        let (build_dir, out, codename) = product_dir();
        let fs = MockFilesystem::default();
        fs.add_file(out.join("README.txt"), "x"); // present, but not a matching zip
        let entry = rom_entry(ROMArtifactMatcher::ZipFilePrefixer, "lineage-");

        let err = BuildService::find_artifact(&fs, &build_dir, &codename, &entry)
            .await
            .expect_err("no matching artifact must end in not_found");
        assert_eq!(err.code(), tonic::Code::NotFound);
    }

    #[tokio::test]
    async fn ok_when_prefixed_zip_present() {
        let (build_dir, out, codename) = product_dir();
        let fs = MockFilesystem::default();
        fs.add_file(out.join("lineage-21-bacon.zip"), "z");
        let entry = rom_entry(ROMArtifactMatcher::ZipFilePrefixer, "lineage-");

        let paths = BuildService::find_artifact(&fs, &build_dir, &codename, &entry)
            .await
            .expect("a matching zip must be found");
        assert_eq!(paths.len(), 1);
        assert!(paths[0].ends_with("lineage-21-bacon.zip"));
    }

    #[tokio::test]
    async fn exact_matcher_not_found_when_file_missing() {
        let (build_dir, _out, codename) = product_dir();
        let fs = MockFilesystem::default(); // file absent
        let entry = rom_entry(ROMArtifactMatcher::ExactMatcher, "boot.img");

        let err = BuildService::find_artifact(&fs, &build_dir, &codename, &entry)
            .await
            .expect_err("a missing exact artifact must end in not_found");
        assert_eq!(err.code(), tonic::Code::NotFound);
    }
}

#[cfg(test)]
mod workflow_tests {
    //! End-to-end failure-path coverage for the build pipeline, driven entirely
    //! by mock seams: "if the build command fails, mark the build failed and end
    //! with an error" — no real processes, repos, or disk.
    use super::runner_tests::MockProcessRunner;
    use super::*;
    use crate::build_common::BuildId;
    use crate::filesystem::mock::MockFilesystem;
    use crate::git_repo::mock::MockGitProvider;
    use crate::rombuild::config_resolver::{ConfigType, ResolvedRomConfig};
    use crate::rombuild::domain::{RomBuildDirs, RomBuildRequest, RomUploadMethod};
    use crate::rombuild::harness::RomBuildHarness;
    use crate::rombuild::types::{
        ManifestBranchesEntry, ManifestEntry, ROMArtifactEntry, ROMBranchEntry,
    };
    use std::collections::VecDeque;

    fn task_with(
        repo_sync: bool,
        runner: Arc<dyn ProcessRunner>,
        git: Arc<dyn GitProvider>,
        fs: Arc<dyn Filesystem>,
        known_builds: Arc<Mutex<Vec<BuildEntry>>>,
    ) -> BuildTask {
        let device_entry = TargetsEntry {
            name: "OnePlus 6".into(),
            codename: "enchilada".into(),
            manufacturer: "oneplus".into(),
        };
        BuildTask {
            domain: RomBuildTask {
                id: BuildId::new("build-test"),
                request: RomBuildRequest {
                    config_name: "cfg".into(),
                    target_codename: "enchilada".into(),
                    rom_name: "lineageos".into(),
                    rom_android_version: 14.0,
                    variant: RomBuildVariant::User,
                    force_checkout: false,
                    parallel_jobs: 1,
                    github_token: None,
                    upload_method: RomUploadMethod::None,
                },
                settings: RomBuildSettings {
                    do_repo_sync: repo_sync,
                    ..RomBuildSettings::default()
                },
                dirs: RomBuildDirs {
                    build: PathBuf::from("/build"),
                    temp: PathBuf::from("/tmp"),
                },
                resolved: ResolvedRomConfig {
                    config_entry: ConfigType::Standard(ManifestEntry {
                        name: "cfg".into(),
                        url: "https://example.com/manifest.git".into(),
                        branches: vec![],
                    }),
                    device_entry,
                    branch_entry: ManifestBranchesEntry {
                        name: "lineage-21".into(),
                        target_rom: "lineageos".into(),
                        android_version: 14.0,
                        device: "enchilada".into(),
                        use_regex: false,
                    },
                    rom_entry: ROMEntry {
                        name: "lineageos".into(),
                        link: String::new(),
                        target: "bacon".into(),
                        artifact: ROMArtifactEntry {
                            matcher: ROMArtifactMatcher::ZipFilePrefixer,
                            data: "lineage-".into(),
                        },
                        branches: vec![],
                    },
                    rom_branch_entry: ROMBranchEntry {
                        android_version: 14.0,
                        branch: "lineage-21".into(),
                    },
                },
            },
            log_tx_clone: broadcast::channel(100).0,
            registry: Arc::new(RomBuildRegistry::with_known(known_builds)),
            askpass_path_clone: None,
            runner,
            git,
            fs,
            kill_rx: mpsc::channel(1).1,
            span: tracing::Span::none(),
        }
    }

    #[tokio::test]
    async fn failed_build_command_marks_failed_and_ends_with_error() {
        // Runner scripted so the (single) build command returns failure.
        let runner = Arc::new(MockProcessRunner {
            calls: Mutex::new(Vec::new()),
            results: Mutex::new(VecDeque::from([false])),
        });
        let git = Arc::new(MockGitProvider::default());
        let fs = Arc::new(MockFilesystem::default());
        let known_builds = Arc::new(Mutex::new(vec![BuildEntry {
            id: "build-test".into(),
            variant: RomBuildVariant::User,
            target_device: TargetsEntry {
                name: "OnePlus 6".into(),
                codename: "enchilada".into(),
                manufacturer: "oneplus".into(),
            },
            config_name: "cfg".into(),
            success: BuildStatus::InProgress,
        }]));

        let task = task_with(false, runner.clone(), git, fs, known_builds.clone());

        // run_build is the spawned task's body, so it always resolves to Ok(());
        // a failure is surfaced by marking the build Failed and ending the
        // pipeline (no upload), not by an Err return.
        let result = RomBuildHarness::run(task).await;
        assert!(
            result.is_ok(),
            "the task wrapper resolves Ok after handling"
        );

        // The build command was actually reached and run...
        let calls = runner.calls.lock().await;
        assert_eq!(calls.len(), 1, "exactly the build command should have run");
        assert_eq!(
            calls[0][0], "bash",
            "the build command is the bash invocation"
        );
        drop(calls);

        // ...and its failure ended the pipeline with the build marked Failed.
        let kb = known_builds.lock().await;
        assert!(
            matches!(kb[0].success, BuildStatus::Failed(_)),
            "a failed build command must mark the build Failed and stop"
        );
    }

    #[tokio::test]
    async fn successful_build_command_marks_success() {
        // Contrast: the same pipeline with the build command succeeding ends in
        // Success — proving the outcome is driven by the command result.
        let runner = Arc::new(MockProcessRunner {
            calls: Mutex::new(Vec::new()),
            results: Mutex::new(VecDeque::from([true])),
        });
        let git = Arc::new(MockGitProvider::default());
        let fs = Arc::new(MockFilesystem::default());
        let known_builds = Arc::new(Mutex::new(vec![BuildEntry {
            id: "build-test".into(),
            variant: RomBuildVariant::User,
            target_device: TargetsEntry {
                name: "OnePlus 6".into(),
                codename: "enchilada".into(),
                manufacturer: "oneplus".into(),
            },
            config_name: "cfg".into(),
            success: BuildStatus::InProgress,
        }]));

        let task = task_with(false, runner.clone(), git, fs, known_builds.clone());
        RomBuildHarness::run(task).await.unwrap();

        assert_eq!(runner.calls.lock().await.len(), 1);
        let kb = known_builds.lock().await;
        assert!(
            matches!(kb[0].success, BuildStatus::Success),
            "a successful build command must mark the build Success"
        );
    }

    #[tokio::test]
    async fn failed_manifest_open_in_repo_phase_ends_build() {
        // Repo phase enabled (program_available mocked true). The manifest repo
        // dir exists, so the pipeline opens it via git — and that open fails.
        let runner = Arc::new(MockProcessRunner::default());
        let git = Arc::new(MockGitProvider {
            fail_open: true,
            ..Default::default()
        });
        let fs = Arc::new(MockFilesystem::default());
        // build_dir is "/build" (see task_with); make the manifest repo "exist"
        // so the open path (not the fresh-clone path) is taken.
        fs.add_dir("/build/.repo/manifests.git");
        let known_builds = Arc::new(Mutex::new(vec![BuildEntry {
            id: "build-test".into(),
            variant: RomBuildVariant::User,
            target_device: TargetsEntry {
                name: "OnePlus 6".into(),
                codename: "enchilada".into(),
                manufacturer: "oneplus".into(),
            },
            config_name: "cfg".into(),
            success: BuildStatus::InProgress,
        }]));

        let task = task_with(true, runner.clone(), git.clone(), fs, known_builds.clone());
        RomBuildHarness::run(task).await.unwrap();

        // The git open was attempted...
        assert!(
            !git.opens.lock().unwrap().is_empty(),
            "the manifest repo open should have been attempted"
        );
        // ...it failed, so the pipeline ended before running any build command...
        assert!(
            runner.calls.lock().await.is_empty(),
            "no build command should run after the git failure"
        );
        // ...and the build is marked Failed.
        let kb = known_builds.lock().await;
        assert!(
            matches!(kb[0].success, BuildStatus::Failed(_)),
            "a git failure in the repo phase must mark the build Failed and stop"
        );
    }
}
