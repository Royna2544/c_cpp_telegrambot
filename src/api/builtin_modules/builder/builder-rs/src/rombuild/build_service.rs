mod grpc_pb {
    include!(concat!(env!("OUT_DIR"), "/tgbot.builder.android.rs"));
}

pub use crate::rombuild::build_service::grpc_pb::rom_build_service_server::RomBuildServiceServer;
use crate::{
    filesystem::{Filesystem, RealFilesystem},
    git_repo::{GitProvider, RealGitProvider},
    gofile_api::upload_file_to_gofile,
    rombuild::{
        build_service::grpc_pb::{
            BuildAction, BuildLogEntry, BuildRequest, BuildResult, BuildSubmission, BuildVariant,
            CleanDirectoryRequest, CleanDirectoryType, DirectoryExistsResponse, LogLevel, Settings,
            UploadMethod, build_result::ResultDetails, rom_build_service_server,
        },
        types::{
            ManifestBranchesEntry, ManifestEntry, ROMArtifactMatcher, ROMBranchEntry,
            ROMBuildConfig, ROMEntry, RecoveryManifestEntry, TargetsEntry,
        },
    },
};

use futures_util::Stream;
#[cfg(unix)]
use nix::libc::{rlimit, setrlimit};
#[cfg(unix)]
use nix::sys::signal::{self, Signal};
#[cfg(unix)]
use nix::unistd::Pid;
use std::{
    fmt::Display,
    path::{Path, PathBuf},
    pin::Pin,
    process::Stdio,
    sync::Arc,
};
use tokio::{
    io::{AsyncBufReadExt, AsyncReadExt, AsyncWriteExt, BufReader},
    process::Command,
    sync::{Mutex, broadcast, mpsc},
    task::JoinHandle,
};
use tokio_stream::wrappers::ReceiverStream;
use tonic::{Request, Response, Status, async_trait};
use tracing::{Instrument, error};
use tracing::{debug, info, warn};
use xml::writer::XmlEvent;

struct ActiveBuild {
    id: String,
    // Used to send "Stop!" signal
    kill_tx: mpsc::Sender<()>,
    // Used to broadcast logs to any connected client
    log_tx: broadcast::Sender<BuildLogEntry>,
    // Used to wait for the task to finish (optional, for cleanup)
    _task: JoinHandle<Result<(), Status>>,
}

#[derive(Clone)]
struct UploadTask {
    method: UploadMethod,
    build_id: String,
    artifact_path: PathBuf,
}

enum BuildStatus {
    InProgress,
    Success,
    Failed(String), // Include error message if failed
}

struct BuildEntry {
    id: String,
    variant: BuildVariant,
    target_device: TargetsEntry,
    config_name: String,
    success: BuildStatus,
}

enum ConfigType {
    Standard(ManifestEntry),
    Recovery(RecoveryManifestEntry),
}

/// Owned state handed to [`BuildService::run_build`] — everything the build
/// pipeline needs, captured up front so the task is `'static` and the pipeline
/// is directly awaitable in tests (inject mock runner/git/fs, drive failures).
struct BuildTask {
    build_settings: Settings,
    build_dir_clone: PathBuf,
    log_tx_clone: broadcast::Sender<BuildLogEntry>,
    uploads_clone: Arc<Mutex<Vec<UploadTask>>>,
    build_id_clone: String,
    tempdir_clone: PathBuf,
    known_builds_clone: Arc<Mutex<Vec<BuildEntry>>>,
    askpass_path_clone: Option<PathBuf>,
    runner: Arc<dyn ProcessRunner>,
    git: Arc<dyn GitProvider>,
    fs: Arc<dyn Filesystem>,
    force_checkout: bool,
    parallel_jobs: i32,
    req: BuildRequest,
    config_entry: ConfigType,
    device_entry: TargetsEntry,
    branch_entry: ManifestBranchesEntry,
    rom_entry: ROMEntry,
    rom_branch_entry: ROMBranchEntry,
    active_job_cleanup: Arc<Mutex<Option<ActiveBuild>>>,
    kill_rx: mpsc::Receiver<()>,
    span: tracing::Span,
}

impl Display for BuildVariant {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let s = match self {
            BuildVariant::User => "user",
            BuildVariant::UserDebug => "userdebug",
            BuildVariant::Eng => "eng",
        };
        write!(f, "{}", s)
    }
}

/// Seam for executing external build commands.
///
/// Production uses [`RealProcessRunner`], which spawns the process and streams
/// its output (the body that used to be `BuildService::run_command_with_logs`).
/// Tests inject a mock so the build workflow can be driven without launching
/// real `repo`/`bash` processes.
#[async_trait]
pub(crate) trait ProcessRunner: Send + Sync {
    async fn run_command_with_logs(
        &self,
        command: Command,
        tx: &broadcast::Sender<BuildLogEntry>,
        kill_rx: Option<&mut mpsc::Receiver<()>>,
        log_path: Option<PathBuf>,
        stdin_rx: Option<mpsc::Receiver<String>>,
    ) -> Result<bool, Status>;
}

/// Real runner that spawns processes — the production implementation.
pub(crate) struct RealProcessRunner;

pub struct BuildService {
    settings: Arc<Mutex<Settings>>,
    build_dir: PathBuf,
    tempdir: PathBuf,
    configs: ROMBuildConfig,
    active_job: Arc<Mutex<Option<ActiveBuild>>>,
    active_uploads: Arc<Mutex<Vec<UploadTask>>>,
    known_builds: Arc<Mutex<Vec<BuildEntry>>>,
    pub shutdown_tx: broadcast::Sender<()>, // Channel to signal shutdown
    // Command-execution seam; defaults to RealProcessRunner, swappable in tests.
    runner: Arc<dyn ProcessRunner>,
    // Git seam; defaults to RealGitProvider, swappable in tests.
    git: Arc<dyn GitProvider>,
    // Filesystem seam; defaults to RealFilesystem, swappable in tests.
    fs: Arc<dyn Filesystem>,
}

impl BuildService {
    fn shell_single_quote(value: &str) -> String {
        format!("'{}'", value.replace('\'', "'\\''"))
    }

    fn configure_repo_command_env(
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

    async fn setup_rbe_env(
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

        let active_job_for_handler = Arc::new(Mutex::new(None::<ActiveBuild>));
        let active_job_clone = active_job_for_handler.clone();

        tokio::spawn(async move {
            tokio::signal::ctrl_c()
                .await
                .expect("Failed to listen for Ctrl-C");
            info!("Global Ctrl-C received");

            // Check if build is active
            let job = active_job_clone.lock().await;
            if let Some(build) = job.as_ref() {
                info!("Cancelling active build: {}", build.id);
                let _ = build.kill_tx.send(()).await;
                drop(job); // Release lock

                // Wait a bit for cleanup
                tokio::time::sleep(tokio::time::Duration::from_secs(5)).await;
            }

            info!("Exiting server");
            std::process::exit(0);
        });

        BuildService {
            settings: Arc::new(Mutex::new(Settings {
                do_repo_sync: Some(true),
                do_clean_build: Some(false),
                use_ccache: Some(false),
                use_rbe_service: Some(false),
                rbe_api_token: None,
                do_upload: Some(false),
            })),
            build_dir,
            tempdir: temp_dir,
            configs,
            active_job: active_job_for_handler,
            active_uploads: Arc::new(Mutex::new(Vec::new())),
            known_builds: Arc::new(Mutex::new(Vec::new())),
            shutdown_tx,
            runner: Arc::new(RealProcessRunner),
            git: Arc::new(RealGitProvider),
            fs: Arc::new(RealFilesystem),
        }
    }

}

#[async_trait]
impl ProcessRunner for RealProcessRunner {
    async fn run_command_with_logs(
        &self,
        mut command: Command, // The configured tokio::process::Command
        tx: &tokio::sync::broadcast::Sender<BuildLogEntry>, // Where to send logs
        mut kill_rx: Option<&mut mpsc::Receiver<()>>, // Optional Kill Switch
        log_path: Option<PathBuf>, // Optional log file path to also write logs to
        stdin_rx: Option<mpsc::Receiver<String>>,
    ) -> Result<bool, Status> {
        // Returns true if success, false if failed/cancelled

        let file_handle = if let Some(path) = log_path {
            // Create file (overwrite if exists).
            // We wrap in Arc<Mutex> so both stdout/stderr tasks can write to it.
            let file = tokio::fs::File::create(&path)
                .await
                .map_err(|e| Status::internal(format!("Failed to create log file: {}", e)))?;
            Some(Arc::new(Mutex::new(file)))
        } else {
            None
        };

        #[cfg(unix)]
        command.process_group(0);
        // 2. Setup Pipes
        command.stdout(Stdio::piped());
        command.stderr(Stdio::piped());
        if stdin_rx.is_some() {
            command.stdin(Stdio::piped());
        } else {
            command.stdin(Stdio::null());
        }

        // 3. Spawn Process
        let mut child = command
            .spawn()
            .map_err(|e| Status::internal(e.to_string()))?;
        let stdout = child.stdout.take().expect("stdout missing");
        let stderr = child.stderr.take().expect("stderr missing");
        if let Some(mut rx) = stdin_rx {
            if let Some(mut child_in) = child.stdin.take() {
                // Spawn a background task to forward messages
                tokio::spawn(async move {
                    while let Some(msg) = rx.recv().await {
                        // Write the message to the process
                        if child_in.write_all(msg.as_bytes()).await.is_err() {
                            break;
                        }

                        // Auto-append newline if missing (Shells usually need Enter)
                        if !msg.ends_with('\n') {
                            if child_in.write_all(b"\n").await.is_err() {
                                break;
                            }
                        }

                        // Flush to ensure the process sees it immediately
                        if child_in.flush().await.is_err() {
                            break;
                        }
                    }
                });
            }
        }

        let mut args = command
            .as_std()
            .get_args()
            .map(|s| s.to_string_lossy().to_string())
            .collect::<Vec<_>>();
        let argv0 = command.as_std().get_program().to_string_lossy().to_string();
        let mut argv = Vec::new();
        argv.push(argv0.clone());
        args = argv.into_iter().chain(args.into_iter()).collect::<Vec<_>>();
        let wd = command
            .as_std()
            .get_current_dir()
            .unwrap_or_else(|| Path::new("<unknown>").into())
            .display()
            .to_string();

        info!("Spawned command: args: [{}], wd: {}", args.join(", "), wd);
        info!("Spawned process with PID: {:?}", child.id());

        // 4. Log Streamers
        let tx_out = tx.clone();
        let tx_err = tx.clone();

        // Clone file handles for the tasks
        let file_out = file_handle.clone();
        let file_err = file_handle.clone();

        // --- Task A: Stdout ---
        let out_handle = tokio::spawn(async move {
            let mut reader = BufReader::new(stdout).lines();
            while let Ok(Some(line)) = reader.next_line().await {
                // A. Write to File (if enabled)
                if let Some(f_arc) = &file_out {
                    // Lock, Write, Newline, Ignore errors
                    let mut f = f_arc.lock().await;
                    let _ = f.write_all(&line.as_bytes()).await;
                    let _ = f.write_all(b"\n").await;
                }

                // B. Send to gRPC
                let _ = tx_out.send(BuildLogEntry {
                    level: LogLevel::Info as i32,
                    message: line,
                    timestamp: chrono::Utc::now().timestamp(),
                    is_finished: false,
                });
            }
        });

        // --- Task B: Stderr ---
        let err_handle = tokio::spawn(async move {
            let mut reader = BufReader::new(stderr).lines();
            while let Ok(Some(line)) = reader.next_line().await {
                // A. Write to File (if enabled)
                if let Some(f_arc) = &file_err {
                    let mut f = f_arc.lock().await;
                    let _ = f.write_all(format!("ERR: {}\n", line).as_bytes()).await;
                }

                // B. Send to gRPC
                let _ = tx_err.send(BuildLogEntry {
                    level: LogLevel::Error as i32,
                    message: line,
                    timestamp: chrono::Utc::now().timestamp(),
                    is_finished: false,
                });
            }
        });

        // 5. Wait for Exit or Kill (Select Logic)
        let success = if let Some(rx) = &mut kill_rx {
            tokio::select! {
                res = child.wait() => {
                    match res {
                        Ok(status) => {
                            info!(
                                "Process exited with status: {}",
                                status.code().unwrap_or(-1)
                            );
                            status.success()
                        }
                        Err(e) => {
                            error!("Failed to wait for process: {}", e);
                            false
                        }
                    }
                },
                _ = rx.recv() => {
                    #[cfg(unix)]
                    {
                        info!("Cancellation requested, sending SIGINT to process group.");
                        if let Some(pid) = child.id() {
                            // Cast u32 -> i32 for the negative PID trick
                            let _ = signal::kill(Pid::from_raw(-(pid as i32)), Signal::SIGINT);
                            info!("Sent SIGINT to process group of PID: {}", pid);
                        }
                    }
                    #[cfg(not(unix))]
                    {
                        info!("Cancellation requested, killing the process.");
                        let _ = child.kill().await;
                    }

                    info!("Waiting for process to exit after cancellation.");
                    let _ = child.wait().await;
                    info!("Process has exited after cancellation.");

                    // Abort log tasks
                    out_handle.abort();
                    err_handle.abort();

                    // Notify cancellation
                    let _ = tx.send(BuildLogEntry {
                        level: LogLevel::Fatal as i32,
                        message: "Build cancelled.".into(),
                        timestamp: chrono::Utc::now().timestamp(),
                        is_finished: true,
                    });
                    // Optional: Log cancellation to file
                    if let Some(f_arc) = &file_handle {
                        let mut f = f_arc.lock().await;
                        let _ = f.write_all(b"\n--- BUILD CANCELLED ---\n").await;
                    }
                    false
                }
            }
        } else {
            match child.wait().await {
                Ok(status) => {
                    info!(
                        "Process exited with status: {}",
                        status.code().unwrap_or(-1)
                    );
                    status.success()
                }
                Err(e) => {
                    error!("Failed to wait for process: {}", e);
                    false
                }
            }
        };

        // Clean up
        let _ = out_handle.await;
        let _ = err_handle.await;

        // File closes automatically when 'file_handle' Arc drops here

        Ok(success)
    }
}

impl BuildService {
    async fn find_artifact(
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

fn log_who_asked_me(method: &str, request: &Request<impl std::fmt::Debug>) {
    if let Some(peer_addr) = request.remote_addr() {
        info!(
            "{} Request received from peer address: {}",
            method, peer_addr
        );
    } else {
        info!("{} Request received from unknown client", method);
    }
}

#[cfg(unix)]
use nix::mount::umount;
#[cfg(unix)]
use std::fs;
#[cfg(unix)]
use std::os::unix::fs::MetadataExt;

#[cfg(unix)]
fn is_mountpoint<P: AsRef<Path>>(path: P) -> std::io::Result<bool> {
    let path = path.as_ref();

    // Get metadata without following symlinks
    let meta = fs::symlink_metadata(path)?;

    // If there is no parent, it's the root directory ("/"),
    // which is always a mountpoint.
    let parent = match path.parent() {
        Some(p) => p,
        None => return Ok(true),
    };

    let parent_meta = fs::symlink_metadata(parent)?;

    // Compare the device IDs
    Ok(meta.dev() != parent_meta.dev())
}

#[cfg(unix)]
fn if_mounted_try_umount<P: AsRef<Path>>(path: P) {
    if is_mountpoint(&path).unwrap_or(false) {
        info!(
            "Path {:?} is a mountpoint, attempting to unmount...",
            path.as_ref()
        );
        match umount(path.as_ref()) {
            Ok(_) => info!("Successfully unmounted {:?}", path.as_ref()),
            Err(e) => error!("Failed to unmount {:?}: {}", path.as_ref(), e),
        }
    } else {
        debug!(
            "Path {:?} is not a mountpoint, no need to unmount.",
            path.as_ref()
        );
    }
}

#[async_trait]
impl rom_build_service_server::RomBuildService for BuildService {
    type StreamLogsStream =
        Pin<Box<dyn Stream<Item = Result<BuildLogEntry, tonic::Status>> + Send + 'static>>;
    type GetBuildResultStream =
        Pin<Box<dyn Stream<Item = Result<BuildResult, tonic::Status>> + Send + 'static>>;

    async fn get_settings(
        &self,
        request: tonic::Request<()>,
    ) -> std::result::Result<tonic::Response<Settings>, tonic::Status> {
        log_who_asked_me("get_settings", &request);
        let settings = self.settings.lock().await;
        Ok(tonic::Response::new(settings.clone()))
    }

    async fn set_settings(
        &self,
        request: tonic::Request<Settings>,
    ) -> std::result::Result<tonic::Response<()>, tonic::Status> {
        log_who_asked_me("set_settings", &request);
        let req = request.into_inner();
        let mut settings = self.settings.lock().await;
        if req.do_repo_sync.is_some() {
            info!("Setting do_repo_sync to {}", req.do_repo_sync.unwrap());
            settings.do_repo_sync = req.do_repo_sync;
        }
        if req.do_clean_build.is_some() {
            info!("Setting do_clean_build to {}", req.do_clean_build.unwrap());
            settings.do_clean_build = req.do_clean_build;
        }
        if req.use_ccache.is_some() {
            info!("Setting use_ccache to {}", req.use_ccache.unwrap());
            settings.use_ccache = req.use_ccache;
        }
        if req.use_rbe_service.is_some() {
            info!(
                "Setting use_rbe_service to {}",
                req.use_rbe_service.unwrap()
            );
            settings.use_rbe_service = req.use_rbe_service;
        }
        if req.rbe_api_token.is_some() {
            info!("Setting rbe_api_token");
            settings.rbe_api_token = req.rbe_api_token;
        }
        if req.do_upload.is_some() {
            info!("Setting do_upload to {}", req.do_upload.unwrap());
            settings.do_upload = req.do_upload;
        }
        Ok(tonic::Response::new(()))
    }

    /// Clean a specified directory
    async fn clean_directory(
        &self,
        request: tonic::Request<CleanDirectoryRequest>,
    ) -> std::result::Result<tonic::Response<()>, tonic::Status> {
        log_who_asked_me("clean_directory", &request);
        let req = request.into_inner();
        let clean_dir = match &req
            .directory_type
            .try_into()
            .map_err(|_| tonic::Status::invalid_argument("Invalid directory type"))?
        {
            CleanDirectoryType::RomDirectory => self.build_dir.clone(),
            CleanDirectoryType::BuildDirectory => self.build_dir.join("out"),
        };
        info!("Cleaning directory: {:?}", clean_dir);
        self.fs
            .remove_dir_all(&clean_dir)
            .map_err(|e| tonic::Status::internal(format!("Failed to clean directory: {}", e)))?;
        Ok(tonic::Response::new(()))
    }

    async fn directory_exists(
        &self,
        request: tonic::Request<CleanDirectoryRequest>,
    ) -> std::result::Result<tonic::Response<DirectoryExistsResponse>, tonic::Status> {
        log_who_asked_me("directory_exists", &request);
        let req = request.into_inner();
        let check_dir = match &req
            .directory_type
            .try_into()
            .map_err(|_| tonic::Status::invalid_argument("Invalid directory type"))?
        {
            CleanDirectoryType::RomDirectory => self.build_dir.clone(),
            CleanDirectoryType::BuildDirectory => self.build_dir.join("out"),
        };
        info!("Checking existence of directory: {:?}", check_dir);
        let exists = self.fs.exists(&check_dir);
        Ok(tonic::Response::new(DirectoryExistsResponse { exists }))
    }

    /// Start a new ROM build.
    async fn start_build(
        &self,
        request: tonic::Request<BuildRequest>,
    ) -> std::result::Result<tonic::Response<BuildSubmission>, tonic::Status> {
        log_who_asked_me("start_build", &request);
        let req = request.into_inner();
        let mut lock = self.active_job.lock().await;

        // Umount /sys/kernel/debug as it causes problem when zipping /d directory later,
        // and it should be safe to umount it since it's only used for debugging and not
        // critical for the build process.
        #[cfg(unix)]
        if_mounted_try_umount("/sys/kernel/debug");

        // 1. Check concurrency (Android builds use 100% CPU, usually 1 at a time is best)
        if lock.is_some() {
            return Ok(Response::new(BuildSubmission {
                build_id: "".into(),
                accepted: false,
                status_message: "A build is already running.".into(),
            }));
        }

        let build_id = format!("build-{}", uuid::Uuid::new_v4());
        info!("Starting new build with ID: {}", build_id);

        // Channel for Cancellation (Capacity 1 is enough)
        let (kill_tx, mut kill_rx) = mpsc::channel::<()>(1);

        // Channel for Logs (Broadcast so multiple clients can watch)
        // Capacity 1000 lines buffer
        let (log_tx, _logz_rx) = broadcast::channel::<BuildLogEntry>(100);

        // Ensure directory exists before we start the build, so that Ctrl-C handler can safely write logs even if the build fails early.
        if !self.fs.exists(&self.build_dir) {
            warn!(
                "Build directory {:?} does not exist, creating it...",
                self.build_dir
            );
            if let Err(e) = self.fs.create_dir_all(&self.build_dir) {
                return Err(Status::internal(format!(
                    "Failed to create build directory: {}",
                    e
                )));
            }
        }

        let active_job_cleanup = self.active_job.clone();

        // First and foremost, locate config entry
        let config_entry = self
            .configs
            .manifests
            .iter()
            .filter(|entry| entry.name == req.config_name)
            .collect::<Vec<_>>();
        let recovery_entry = self
            .configs
            .recovery_manifests
            .iter()
            .filter(|entry| entry.name == req.config_name)
            .collect::<Vec<_>>();
        if config_entry.len() != 1 && recovery_entry.len() != 1 {
            return Err(tonic::Status::invalid_argument(format!(
                "No unique configuration found with name: {} (Got {} entries)",
                req.config_name,
                config_entry.len()
            )));
        }

        let config_entry = if config_entry.len() == 1 {
            ConfigType::Standard(config_entry[0].clone())
        } else {
            ConfigType::Recovery(recovery_entry[0].clone())
        };

        let device_entry = self
            .configs
            .targets
            .iter()
            .filter(|entry| entry.codename == req.target_device)
            .collect::<Vec<_>>();
        if device_entry.len() != 1 {
            return Err(tonic::Status::invalid_argument(format!(
                "No unique device found with codename: {} (Got {} entries)",
                req.target_device,
                device_entry.len()
            )));
        }
        let device_entry = device_entry[0].clone();
        info!("Found device entry: {}", device_entry.codename);

        let (branch_entry, rom_entry, rom_branch_entry): (
            ManifestBranchesEntry,
            ROMEntry,
            ROMBranchEntry,
        ) = match config_entry {
            ConfigType::Standard(ref cfg) => {
                let branches = cfg
                    .branches
                    .iter()
                    .filter(|entry| {
                        entry.target_rom == req.rom_name
                            && entry.android_version == req.rom_android_version
                            && (entry.device == req.target_device
                                || (entry.use_regex
                                    && regex::Regex::new(&entry.device)
                                        .map(|re| re.is_match(&req.target_device))
                                        .unwrap_or(false)))
                    })
                    .collect::<Vec<_>>();
                if branches.len() != 1 {
                    for b in &branches {
                        info!("Matching branch found: {} for device: {}", b.name, b.device);
                    }
                    return Err(tonic::Status::invalid_argument(format!(
                        "No unique branch found for target device: {} (Got {} entries)",
                        req.target_device,
                        branches.len()
                    )));
                }
                let branch_entry = branches[0];
                info!(
                    "Found local manifest branch entry: {} for ROM: {}",
                    branch_entry.name, branch_entry.target_rom
                );

                let rom_entry = self
                    .configs
                    .roms
                    .iter()
                    .filter(|entry| entry.name == branch_entry.target_rom)
                    .collect::<Vec<_>>();
                if rom_entry.len() != 1 {
                    for r in &rom_entry {
                        info!("Matching ROM found: {}", r.name);
                    }
                    return Err(tonic::Status::invalid_argument(format!(
                        "No unique ROM found with name: {} (Got {} entries)",
                        branch_entry.target_rom,
                        rom_entry.len()
                    )));
                }
                let rom_entry = rom_entry[0];
                info!("Found ROM entry: {}", rom_entry.name);

                let rom_branch_entry = rom_entry
                    .branches
                    .iter()
                    .filter(|entry| entry.android_version == branch_entry.android_version)
                    .collect::<Vec<_>>();
                if rom_branch_entry.len() != 1 {
                    for r in &rom_branch_entry {
                        info!("Matching ROM branch found: {}", r.branch);
                    }
                    return Err(tonic::Status::invalid_argument(format!(
                        "No unique ROM branch found with name: {} (Got {} entries)",
                        branch_entry.name,
                        rom_branch_entry.len()
                    )));
                }
                let rom_branch_entry = rom_branch_entry[0];
                info!("Found ROM branch entry: {}", &rom_branch_entry.branch);
                (
                    branch_entry.clone(),
                    rom_entry.clone(),
                    rom_branch_entry.clone(),
                )
            }
            ConfigType::Recovery(ref cfg) => {
                let recoveries = self
                    .configs
                    .recoveries
                    .iter()
                    .filter(|entry| entry.name == cfg.target_recovery)
                    .collect::<Vec<_>>();
                if recoveries.len() != 1 {
                    return Err(tonic::Status::invalid_argument(format!(
                        "No unique recovery branch found with name: {} (Got {} entries)",
                        cfg.target_recovery,
                        recoveries.len()
                    )));
                }
                let recovery = recoveries[0];
                info!(
                    "Found recovery entry: {} for name: {}",
                    &recovery.name, &cfg.name
                );
                let branches = recovery
                    .branches
                    .iter()
                    .filter(|entry| entry.android_version == cfg.android_version)
                    .collect::<Vec<_>>();
                if branches.len() != 1 {
                    return Err(tonic::Status::invalid_argument(format!(
                        "No unique branch found for recovery: {} (Got {} entries)",
                        cfg.target_recovery,
                        branches.len()
                    )));
                }
                let rom_branch_entry = branches[0];

                let branch_entry = ManifestBranchesEntry {
                    name: String::new(), // Placeholder, not used
                    target_rom: cfg.target_recovery.clone(),
                    android_version: rom_branch_entry.android_version,
                    device: cfg.device.clone(),
                    use_regex: cfg.use_regex,
                };
                (branch_entry, recovery.clone(), rom_branch_entry.clone())
            }
        };

        let parallel_jobs = match req.parallel_jobs {
            Some(jobs) => {
                info!("Using {} parallel jobs for build", jobs);
                jobs
            }
            None => num_cpus::get() as i32,
        };

        let build_variant_enum = req.build_variant.try_into().map_err(|_| {
            tonic::Status::invalid_argument("Invalid build variant specified in request")
        })?;

        let known_builds_entry = BuildEntry {
            id: build_id.clone(),
            variant: build_variant_enum,
            target_device: device_entry.clone(),
            config_name: req.config_name.clone(),
            success: BuildStatus::InProgress,
        };
        self.known_builds.lock().await.push(known_builds_entry);

        // Write git-askpass file if github token is provided
        let askpass_path = if let Some(token) = &req.github_token {
            let askpass_path = self.build_dir.join("git-askpass.sh");
            info!("Writing git-askpass file to {:?}", askpass_path);
            let script = format!(
                "#!/bin/sh\nprintf '%s\\n' {}\n",
                Self::shell_single_quote(token)
            );
            let res = self.fs.write(&askpass_path, script.as_bytes());
            if let Err(e) = res {
                return Err(Status::internal(format!(
                    "Failed to write git-askpass file: {}",
                    e
                )));
            }
            // Make it executable
            #[cfg(unix)]
            {
                self.fs.set_mode(&askpass_path, 0o700).map_err(|e| {
                    tonic::Status::internal(format!(
                        "Failed to set permissions for git-askpass file: {}",
                        e
                    ))
                })?;
            }
            Some(askpass_path)
        } else {
            None
        };

        let build_settings = self.settings.lock().await.clone();
        let build_dir_clone = self.build_dir.clone();
        let log_tx_clone = log_tx.clone();
        let uploads_clone = self.active_uploads.clone();
        let build_id_clone = build_id.clone();
        let tempdir_clone = self.tempdir.clone();
        let known_builds_clone = self.known_builds.clone();
        let askpass_path_clone = askpass_path.clone();
        let runner = self.runner.clone();
        let git = self.git.clone();
        let fs = self.fs.clone();
        let force_checkout = req.force_checkout.unwrap_or(false);

        let span = tracing::info_span!("build_task", build_id = build_id);
        let task = BuildTask {
            build_settings,
            build_dir_clone,
            log_tx_clone,
            uploads_clone,
            build_id_clone,
            tempdir_clone,
            known_builds_clone,
            askpass_path_clone,
            runner,
            git,
            fs,
            force_checkout,
            parallel_jobs,
            req,
            config_entry,
            device_entry,
            branch_entry,
            rom_entry,
            rom_branch_entry,
            active_job_cleanup,
            kill_rx,
            span,
        };
        let task_handle: tokio::task::JoinHandle<Result<(), Status>> =
            tokio::spawn(Self::run_build(task));

        *lock = Some(ActiveBuild {
            id: build_id.clone(),
            kill_tx,
            log_tx,
            _task: task_handle,
        });

        info!("Build with ID: {} has been queued.", build_id);

        Ok(Response::new(BuildSubmission {
            build_id,
            accepted: true,
            status_message: "Build queued successfully.".into(),
        }))
    }

    /// Logs streaming for a build in progress
    async fn stream_logs(
        &self,
        request: tonic::Request<BuildAction>,
    ) -> std::result::Result<tonic::Response<Self::StreamLogsStream>, tonic::Status> {
        log_who_asked_me("stream_logs", &request);
        let req = request.into_inner();
        let lock = self.active_job.lock().await;

        let active_build = match lock.as_ref() {
            Some(build) if build.id == req.build_id => build,
            _ => {
                return Err(tonic::Status::invalid_argument(
                    "No active build with the specified ID.",
                ));
            }
        };

        let mut log_rx = active_build.log_tx.subscribe();

        let output = async_stream::try_stream! {
            loop {
                let log_entry = log_rx.recv().await.map_err(|e| {
                    tonic::Status::internal(format!("Failed to receive log entry: {}", e))
                })?;
                if log_entry.is_finished {
                    break;
                }
                yield log_entry;
            }
        };

        Ok(tonic::Response::new(
            Box::pin(output) as Self::StreamLogsStream
        ))
    }

    /// Cancel a build in progress
    async fn cancel_build(
        &self,
        request: tonic::Request<BuildAction>,
    ) -> std::result::Result<tonic::Response<()>, tonic::Status> {
        log_who_asked_me("cancel_build", &request);
        let req = request.into_inner();
        let lock = self.active_job.lock().await;

        let active_build = match lock.as_ref() {
            Some(build) if build.id == req.build_id => build,
            _ => {
                return Err(tonic::Status::invalid_argument(
                    "No active build with the specified ID.",
                ));
            }
        };

        info!("Cancelling build with ID: {}", req.build_id);
        // Send cancellation signal
        match active_build.kill_tx.send(()).await {
            Ok(_) => {
                info!("Cancellation signal sent successfully.");
                Ok(tonic::Response::new(()))
            }
            Err(e) => {
                error!("Failed to send cancellation signal: {}", e);
                Err(tonic::Status::internal(
                    "Failed to send cancellation signal.",
                ))
            }
        }
    }

    async fn get_status(
        &self,
        request: tonic::Request<BuildAction>,
    ) -> std::result::Result<tonic::Response<BuildSubmission>, tonic::Status> {
        log_who_asked_me("get_status", &request);
        let req = request.into_inner();
        let lock = self.active_job.lock().await;

        let (accepted, status_message) = match lock.as_ref() {
            Some(build) if build.id == req.build_id => (true, "Build is in progress.".into()),
            _ => (false, "No active build with the specified ID.".into()),
        };

        Ok(tonic::Response::new(BuildSubmission {
            build_id: req.build_id,
            accepted,
            status_message,
        }))
    }

    async fn get_build_result(
        &self,
        request: tonic::Request<BuildAction>,
    ) -> std::result::Result<tonic::Response<Self::GetBuildResultStream>, tonic::Status> {
        log_who_asked_me("get_build_result", &request);
        let req = request.into_inner();
        let known_builds = self.known_builds.lock().await;

        let build_entry = known_builds
            .iter()
            .find(|entry| entry.id == req.build_id)
            .ok_or_else(|| {
                tonic::Status::invalid_argument("No known build with the specified ID.")
            })?;

        info!("Fetching build result for ID: {}", req.build_id);
        info!(
            "Found build entry: configname: {}, device: {}, variant: {}",
            build_entry.config_name, build_entry.target_device.name, build_entry.variant
        );

        // Create a channel to stream results
        let (tx, rx) = mpsc::channel(10);

        match &build_entry.success {
            BuildStatus::Failed(error_message) => {
                warn!("Entering failure branch for build ID: {}", req.build_id);
                tx.send(Ok(BuildResult {
                    success: false,
                    upload_method: UploadMethod::None as i32,
                    result_details: None,
                    error_message: Some(error_message.clone()),
                    file_name: None,
                }))
                .await
                .map_err(|e| {
                    tonic::Status::internal(format!("Failed to send failure build result: {}", e))
                })?;
                return Ok(tonic::Response::new(
                    Box::pin(ReceiverStream::new(rx)) as Self::GetBuildResultStream
                ));
            }
            BuildStatus::Success => {
                info!("Build marked as successful for ID: {}", req.build_id);
            }
            BuildStatus::InProgress => {
                info!("Build is still in progress for ID: {}", req.build_id);
                return Err(tonic::Status::failed_precondition(
                    "Build is still in progress. Please check back later for results.",
                ));
            }
        }

        let upload_tasks = self.active_uploads.lock().await;
        let upload_task = {
            let upload_task_ = upload_tasks
                .iter()
                .find(|task| task.build_id == req.build_id);
            if !upload_task_.is_some() {
                tx.send(Ok(BuildResult {
                    success: true,
                    upload_method: UploadMethod::None as i32,
                    result_details: None,
                    error_message: Some("No upload task found for this build.".into()),
                    file_name: None,
                }))
                .await
                .map_err(|e| {
                    tonic::Status::internal(format!("Failed to send build result: {}", e))
                })?;
                // No upload task found, but build is marked as successful. This can happen if upload is disabled in settings,
                // or if the artifact was not found. Just return success without upload details.
                return Ok(tonic::Response::new(
                    Box::pin(ReceiverStream::new(rx)) as Self::GetBuildResultStream
                ));
            }
            let upload_task = upload_task_.unwrap().clone();
            drop(upload_tasks);
            upload_task
        };
        info!("Found upload task for build ID: {}", req.build_id);

        let file_name = upload_task
            .artifact_path
            .file_name()
            .ok_or_else(|| {
                tonic::Status::internal(format!(
                    "Invalid artifact path: {}",
                    upload_task.artifact_path.display()
                ))
            })?
            .to_string_lossy()
            .to_string();
        info!("Artifact file name: {}", &file_name);

        tokio::spawn(async move {
            match upload_task.method {
                UploadMethod::LocalFile => {
                    info!(
                        "Returning LocalFile build result for build ID: {}",
                        req.build_id
                    );
                    tx.send(Ok(BuildResult {
                        success: true,
                        upload_method: UploadMethod::LocalFile as i32,
                        result_details: Some(ResultDetails::LocalFilePath(
                            upload_task.artifact_path.to_string_lossy().to_string(),
                        )),
                        error_message: None,
                        file_name: Some(file_name.clone()),
                    }))
                    .await
                    .map_err(|e| {
                        tonic::Status::internal(format!(
                            "Failed to send LocalFile build result: {}",
                            e
                        ))
                    })?;
                }
                UploadMethod::Stream => {
                    info!(
                        "Returning Stream build result for build ID: {}",
                        req.build_id
                    );
                    let mut file = tokio::fs::File::open(&upload_task.artifact_path)
                        .await
                        .map_err(|e| {
                            tonic::Status::internal(format!(
                                "Failed to open artifact file for streaming: {}",
                                e
                            ))
                        })?;
                    let mut buffer = vec![0u8; 1024 * 1024]; // 1MB buffer
                    loop {
                        let n = file.read(&mut buffer).await;
                        if let Err(_e) = n {
                            tx.send(Ok(BuildResult {
                                success: false,
                                upload_method: UploadMethod::Stream as i32,
                                result_details: None,
                                error_message: None,
                                file_name: None,
                            }))
                            .await
                            .map_err(|e| {
                                tonic::Status::internal(format!(
                                    "Failed to send Stream build result: {}",
                                    e
                                ))
                            })?;
                            break;
                        }
                        let n = n.unwrap();
                        if n == 0 {
                            break;
                        }
                        tx.send(Ok(BuildResult {
                            success: true,
                            upload_method: UploadMethod::Stream as i32,
                            result_details: Some(ResultDetails::StreamData(buffer[..n].to_vec())),
                            error_message: None,
                            file_name: Some(file_name.clone()),
                        }))
                        .await
                        .map_err(|e| {
                            tonic::Status::internal(format!(
                                "Failed to send Stream build result: {}",
                                e
                            ))
                        })?;
                    }
                }
                UploadMethod::GoFile => {
                    info!(
                        "Returning GoFile build result for build ID: {}",
                        req.build_id
                    );
                    let upload_response =
                        upload_file_to_gofile(&upload_task.artifact_path.to_string_lossy())
                            .await
                            .map_err(|e| {
                                tonic::Status::internal(format!(
                                    "Failed to upload file to GoFile: {}",
                                    e
                                ))
                            })?;
                    // Here we would normally have the link from the upload process
                    let gofile_link = upload_response.data.downloadPage;
                    info!("File uploaded to GoFile successfully: {}", gofile_link);
                    tx.send(Ok(BuildResult {
                        success: true,
                        upload_method: UploadMethod::GoFile as i32,
                        result_details: Some(ResultDetails::GofileLink(gofile_link)),
                        error_message: None,
                        file_name: Some(file_name),
                    }))
                    .await
                    .map_err(|e| {
                        tonic::Status::internal(format!(
                            "Failed to send GoFile build result: {}",
                            e
                        ))
                    })?;
                }
                _ => {
                    tx.send(Ok(BuildResult {
                        success: false,
                        upload_method: UploadMethod::None as i32,
                        result_details: None,
                        error_message: Some("Unsupported upload method.".into()),
                        file_name: None,
                    }))
                    .await
                    .map_err(|e| {
                        tonic::Status::internal(format!("Failed to send None build result: {}", e))
                    })?;
                }
            }
            Ok::<(), tonic::Status>(())
        });

        Ok(tonic::Response::new(
            Box::pin(ReceiverStream::new(rx)) as Self::GetBuildResultStream
        ))
    }
}

impl BuildService {
    /// The build pipeline, extracted from `start_build`'s spawned task so it is
    /// directly awaitable: tests inject mock runner/git/fs via `BuildTask` and
    /// assert how a failing command / git / filesystem op is handled and ends.
    async fn run_build(task: BuildTask) -> Result<(), Status> {
        let BuildTask {
            build_settings,
            build_dir_clone,
            log_tx_clone,
            uploads_clone,
            build_id_clone,
            tempdir_clone,
            known_builds_clone,
            askpass_path_clone,
            runner,
            git,
            fs,
            force_checkout,
            parallel_jobs,
            req,
            config_entry,
            device_entry,
            branch_entry,
            rom_entry,
            rom_branch_entry,
            active_job_cleanup,
            mut kill_rx,
            span,
        } = task;
            macro_rules! send_log {
                ($level:expr, $msg:expr) => {
                    match $level {
                        LogLevel::Debug => info!("{}", $msg),
                        LogLevel::Info => info!("{}", $msg),
                        LogLevel::Warning => warn!("{}", $msg),
                        LogLevel::Error => error!("{}", $msg),
                        LogLevel::Fatal => error!("{}", $msg),
                    }
                    let _ = log_tx_clone
                        .send(BuildLogEntry {
                            level: $level.into(),
                            message: $msg,
                            timestamp: chrono::Utc::now().timestamp(),
                            is_finished: false,
                        })
                        .inspect_err(|e| {
                            error!("Failed to send log entry: {}", e);
                        });
                };
            }
            macro_rules! send_log_final {
                ($level:expr, $msg:expr) => {
                    match $level {
                        LogLevel::Debug => info!("{}", $msg),
                        LogLevel::Info => info!("{}", $msg),
                        LogLevel::Warning => warn!("{}", $msg),
                        LogLevel::Error => error!("{}", $msg),
                        LogLevel::Fatal => error!("{}", $msg),
                    }
                    let _ = log_tx_clone
                        .send(BuildLogEntry {
                            level: $level.into(),
                            message: $msg,
                            timestamp: chrono::Utc::now().timestamp(),
                            is_finished: true,
                        })
                        .inspect_err(|e| {
                            error!("Failed to send final log entry: {}", e);
                        });
                };
            }

            let res = async {
                let build_log_filename_suffix = format!("build-{}.log", &build_id_clone);
                // First, check if repo command is available
                if build_settings.do_repo_sync.unwrap_or(false) {
                    send_log!(LogLevel::Debug, "Checking for 'repo' command availability...".to_string());
                    if which::which("repo").is_err() {
                        return Err(tonic::Status::failed_precondition(
                            "The 'repo' command is not available in the system PATH.",
                        ));
                    }
                    send_log!(LogLevel::Debug, "'repo' command is available.".to_string());

                    // Open .repo/manifest git repository and check URL and branch
                    let mut need_reinit = false;
                    let manifest_repo_path = &build_dir_clone.join(".repo").join("manifests.git");
                    if fs.exists(&manifest_repo_path) {
                        send_log!(LogLevel::Debug, format!("Opening manifest git repository at {:?}", manifest_repo_path));
                        let repo = git.open(&manifest_repo_path, "origin", None, None)
                            .map_err(|e| {
                                tonic::Status::internal(format!(
                                    "Failed to open manifest git repository: {}",
                                    e
                                ))
                            })?;

                        let repo_url = repo.get_remote_url().map_err(|x| {
                            Status::internal(format!("Cannot retrieve remote-url: {}", x))
                        })?;
                        let branch_name = repo.get_branch_name().map_err(|x| {
                            Status::internal(format!("Cannot retrieve branch name: {}", x))
                        })?;

                        send_log!(LogLevel::Debug, format!("manifest repo url:{} branch:{}", repo_url, branch_name));

                        // On a valid repo init result, the .repo/manifests.git should match the expected URL *BUT* branch name is always 'default'
                        // So we have to check the HEAD and remote's branch named "<rom_branch_entry.branch>"
                        if repo_url.trim_end_matches('/') != rom_entry.link.trim_end_matches('/') || !repo.cmp_head_with_remote_branch(&rom_branch_entry.branch).unwrap_or(false) {
                            send_log!(LogLevel::Info, format!(
                                "Manifest repo URL or branch mismatch. Expected URL: {}, branch: {}. Found URL: {}, branch: {}. Will re-initialize repo.",
                                rom_entry.link, rom_branch_entry.branch, repo_url, branch_name
                            ));
                            // Set flag to re-init repo
                            need_reinit = true;
                        }
                    } else {
                        send_log!(LogLevel::Info, format!(
                            "No manifest git repository found at {:?}. Will initialize repo.",
                            manifest_repo_path
                        ));
                        // Set flag to re-init repo
                        need_reinit = true;
                    }

                    if need_reinit {
                        send_log!(LogLevel::Info, "Initializing repo...".to_string());

                        let mut repo_init_cmd = Command::new("repo");
                        repo_init_cmd.arg("init");
                        repo_init_cmd.arg("-u");
                        repo_init_cmd.arg(&rom_entry.link);
                        repo_init_cmd.arg("-b");
                        repo_init_cmd.arg(&rom_branch_entry.branch);
                        repo_init_cmd.arg("--git-lfs");
                        repo_init_cmd.arg("--depth=1");
                        repo_init_cmd.current_dir(&build_dir_clone);
                        Self::configure_repo_command_env(
                            &mut repo_init_cmd,
                            &build_dir_clone,
                            askpass_path_clone.as_deref(),
                        );

                        let (stdin_tx, stdin_rx) = mpsc::channel(10);

                        // Sometimes, repo init may ask to "enable colored output" --- we auto-confirm it.
                        stdin_tx
                            .send("y".into())
                            .await
                            .map_err(|e| tonic::Status::internal(format!("Failed to send to stdin: {}", e)))?;

                        let error_file = (&tempdir_clone).join(format!("{}-{}", "repo-init", &build_log_filename_suffix));
                        info!("Repo init output log path: {:?}", &error_file);

                        let res = runner.run_command_with_logs(
                            repo_init_cmd,
                            &log_tx_clone,
                            Some(&mut kill_rx),
                            Some(error_file.clone()),
                            stdin_rx.into(),
                        ).await?;
                        if !res {
                            // Update known builds entry to contain failure
                            let known_builds_self = &mut known_builds_clone.lock().await;
                            if let Some(build_entry) = known_builds_self.iter_mut().find(|b| b.id == build_id_clone) {
                                let content = fs.read_to_string(&error_file).unwrap_or_else(|_| "Failed to read error log.".to_string());
                                // Remove error log file after reading
                                fs.remove_file(&error_file).unwrap_or_else(|e| {
                                    error!("Failed to remove error log file {:?}: {}", &error_file, e);
                                });
                                build_entry.success = BuildStatus::Failed(content);
                            }
                            return Err(tonic::Status::internal("'repo init' command failed or was cancelled."));
                        }
                    }

                    // Prepare local manifest
                    send_log!(LogLevel::Info, "Preparing local manifest...".to_string());
                    // Switches to two cases: One with full prebuilt manifest url, one that we have to write it ourselves.
                    let local_manifest_dir = &build_dir_clone.join(".repo").join("local_manifests");
                    match config_entry {
                        ConfigType::Standard(rom) => {
                            match git.open(
                                &local_manifest_dir,
                                "origin",
                                req.github_token.clone(),
                                None,
                            ) {
                                Ok(repo) => {
                                    if repo.get_remote_url().map_err(|e| {
                                        tonic::Status::internal(format!(
                                            "Failed to get remote URL of local manifest repository: {}",
                                            e
                                        ))
                                    })? != rom.url
                                    {
                                        send_log!(LogLevel::Warning, "Local manifest repository URL mismatch, re-cloning...".to_string());
                                        fs.remove_dir_all(&local_manifest_dir).map_err(|e| {
                                            tonic::Status::internal(format!(
                                                "Failed to remove existing local manifest directory: {}",
                                                e
                                            ))
                                        })?;

                                        git.clone_repo(
                                            &rom.url,
                                            &branch_entry.name,
                                            None,
                                            &PathBuf::from(local_manifest_dir),
                                            req.github_token.clone(),
                                            &None,
                                        )
                                        .map_err(|e| {
                                            tonic::Status::internal(format!(
                                                "Failed to clone local manifest repository: {}",
                                                e
                                            ))
                                        })?;
                                    } else {
                                        send_log!(LogLevel::Info, "Local manifest repository URL matches expected URL.".to_string());
                                        if &repo.get_branch_name().map_err(|e| {
                                            tonic::Status::internal(format!(
                                                "Failed to get branch name of local manifest repository: {}",
                                                e
                                            ))
                                        })? != &branch_entry.name {
                                            send_log!(LogLevel::Warning, "Local manifest repository branch mismatch, checking out correct branch...".to_string());
                                            repo.checkout_branch(&branch_entry.name).map_err(|e| {
                                                tonic::Status::internal(format!(
                                                    "Failed to checkout branch {} of local manifest repository: {}",
                                                    &branch_entry.name, e
                                                ))
                                            })?;
                                        } else {
                                            send_log!(LogLevel::Info, String::from("Local manifest repository branch matches expected branch."));
                                        }
                                    send_log!(LogLevel::Info, String::from("Fast-forwarding local manifest repository..."));
                                    let _ = repo
                                        .fast_forward()
                                        .inspect_err(|e| {
                                            send_log!(LogLevel::Error, format!("Failed to fast-forward: {}", e)); 
                                    });
                                    }
                                }
                                Err(_) => {
                                    send_log!(LogLevel::Info, format!("Cloning local manifest repository from {}...", &rom.url));
                                    git.clone_repo(
                                        &rom.url,
                                        &branch_entry.name,
                                        None,
                                        &PathBuf::from(local_manifest_dir),
                                        req.github_token.clone(),
                                        &None,
                                    )
                                    .map_err(|e| {
                                        tonic::Status::internal(format!(
                                            "Failed to clone local manifest repository: {}",
                                            e
                                        ))
                                    })?;
                                }
                            }

                            // Handle custom attribute: recurse_submodules:bool on the manifest entry
                            for path in fs.read_dir(&local_manifest_dir).map_err(|e| {
                                tonic::Status::internal(format!(
                                    "Failed to read local manifests directory: {}",
                                    e
                                ))
                            })?
                            {
                                if path.extension().and_then(|s| s.to_str()) == Some("xml") {
                                    // Parse XML to check for recurse_submodules attribute
                                    let content = fs.read_to_string(&path).map_err(|e| {
                                        tonic::Status::internal(format!(
                                            "Failed to read local manifest file {:?}: {}",
                                            path, e
                                        ))
                                    })?;
                                    let parser = xml::reader::EventReader::from_str(&content);
                                    for e in parser {
                                        match e {
                                            Ok(xml::reader::XmlEvent::StartElement { name, attributes, .. }) if name.local_name == "project" => {
                                                for attr in &attributes {
                                                    if attr.name.local_name == "recurse_submodules" && attr.value == "true" {
                                                        send_log!(LogLevel::Info, format!("Found recurse_submodules=true in manifest file {:?}, updating submodules...", path));

                                                        let sub_repo_name = attributes.iter()
                                                                .find(|attr| attr.name.local_name == "name").map(|attr| attr.value.clone());
                                                        if sub_repo_name.is_none() {
                                                            send_log!(LogLevel::Warning, format!("recurse_submodules=true is set but no project path found in manifest file {:?}, skipping submodule update.", path));
                                                            continue;
                                                        }
                                                        let sub_repo_path = attributes.iter()
                                                                .find(|attr| attr.name.local_name == "path").map(|attr| attr.value.clone());
                                                        if sub_repo_path.is_none() {
                                                            send_log!(LogLevel::Warning, format!("recurse_submodules=true is set but no project path found in manifest file {:?}, skipping submodule update.", path));
                                                            continue;
                                                        }

                                                        let sub_repo_name = sub_repo_name.unwrap();
                                                        let sub_repo_path = sub_repo_path.unwrap();

                                                        // Perform a partial repo sync for the submodule to initialize the .git directory, then we can use git commands to update submodules.
                                                        let mut repo_sync_command = Command::new("repo");
                                                        repo_sync_command.args([
                                                                "sync",
                                                                "-c",
                                                                "--force-sync",
                                                                "--no-clone-bundle",
                                                                "--no-tags",
                                                                format!("-j{}", parallel_jobs.to_string()).as_str(),
                                                                &(&sub_repo_name.clone()),
                                                            ])
                                                            .current_dir(&build_dir_clone);
                                                        Self::configure_repo_command_env(
                                                            &mut repo_sync_command,
                                                            &build_dir_clone,
                                                            askpass_path_clone.as_deref(),
                                                        );
                                                        let error_file = (&tempdir_clone).join(format!("{}-{}-submodule-sync.log", "repo-sync", &build_id_clone));
                                                        info!("Repo sync for submodule output log path: {:?}", &error_file);
                                                        let repo_sync_status = runner.run_command_with_logs(
                                                            repo_sync_command,
                                                            &log_tx_clone,
                                                            Some(&mut kill_rx),
                                                            Some(error_file.clone()),
                                                            None,
                                                        ).await?;
                                                        if !repo_sync_status {
                                                            send_log!(LogLevel::Warning, format!("'repo sync' for submodule {} failed or was cancelled.", &sub_repo_name));
                                                            continue;
                                                        }

                                                        let sub_repo = git.open(
                                                            &PathBuf::from(build_dir_clone.join(&sub_repo_path)),
                                                            "origin",
                                                            req.github_token.clone(),
                                                            None,
                                                        ).map_err(|e| {
                                                            tonic::Status::internal(format!(
                                                                "Failed to open local manifest git repository for submodule update: {}",
                                                                e
                                                            ))
                                                        })?;
                                                        sub_repo.update_modules().map_err(|e| {
                                                            send_log!(LogLevel::Warning, format!("Git submodule command failed with status: {:?}", e));
                                                            tonic::Status::internal(format!(
                                                                "Failed to update git submodules: {}",
                                                                e
                                                            ))
                                                        })?;
                                                    }
                                                }
                                            }
                                            Err(e) => {
                                                send_log!(LogLevel::Error, format!("Error parsing XML in manifest file {:?}: {}", path, e));
                                            }
                                            _ => {}
                                        }
                                    }
                                }
                            }
                        }
                        ConfigType::Recovery(recov) => {
                            if !fs.exists(&local_manifest_dir) {
                                fs.create_dir_all(&local_manifest_dir).map_err(|e| {
                                    tonic::Status::internal(format!(
                                        "Failed to create local manifests directory: {}",
                                        e
                                    ))
                                })?;
                            }
                            let local_manifest_path = local_manifest_dir.join("rombuilder-rs.xml");
                            let mut xml_doc = xml::writer::EmitterConfig::new()
                                .perform_indent(true)
                                .create_writer(std::fs::File::create(&local_manifest_path).map_err(
                                    |e| {
                                        tonic::Status::internal(format!(
                                            "Failed to create local manifest file: {}",
                                            e
                                        ))
                                    },
                                )?);
                            xml_doc
                                .write(XmlEvent::StartDocument {
                                    version: xml::common::XmlVersion::Version10,
                                    encoding: Some("UTF-8"),
                                    standalone: Some(true),
                                })
                                .map_err(|e| {
                                    tonic::Status::internal(format!(
                                        "Failed to write XML start document: {}",
                                        e
                                    ))
                                })?;

                            // Scheme:
                            // <manifest>
                            // <remote name="cppbot_github" fetch="https://github.com"/>
                            // <project name="recovery_manifest_name" path="." revision="branch_name"/>
                            // </manifest>
                            xml_doc
                                .write(XmlEvent::start_element("manifest"))
                                .map_err(|e| {
                                    tonic::Status::internal(format!(
                                        "Failed to write XML start element: {}",
                                        e
                                    ))
                                })?;
                            xml_doc
                                .write(
                                    XmlEvent::start_element("remote")
                                        .attr("name", "cppbot_github")
                                        .attr("fetch", "https://github.com"),
                                )
                                .map_err(|e| {
                                    tonic::Status::internal(format!(
                                        "Failed to write XML start element: {}",
                                        e
                                    ))
                                })?;
                            xml_doc
                                .write(XmlEvent::end_element()) // remote
                                .map_err(|e| {
                                    tonic::Status::internal(format!(
                                        "Failed to write XML end element: {}",
                                        e
                                    ))
                                })?;

                            for recov in &recov.clone_mappings {
                                xml_doc
                                    .write(
                                        XmlEvent::start_element("project")
                                            .attr("name", &recov.repo)
                                            .attr("path", &recov.path)
                                            .attr("revision", &recov.branch)
                                            .attr("remote", "cppbot_github"),
                                    )
                                    .map_err(|e| {
                                        tonic::Status::internal(format!(
                                            "Failed to write XML start element: {}",
                                            e
                                        ))
                                    })?;
                                xml_doc
                                    .write(XmlEvent::end_element()) // project
                                    .map_err(|e| {
                                        tonic::Status::internal(format!(
                                            "Failed to write XML end element: {}",
                                            e
                                        ))
                                    })?;
                            }
                            xml_doc
                                .write(XmlEvent::end_element()) // manifest
                                .map_err(|e| {
                                    tonic::Status::internal(format!(
                                        "Failed to write XML end element: {}",
                                        e
                                    ))
                                })?;
                            send_log!(LogLevel::Info, format!("Written local manifest to {:?}", local_manifest_path));
                        }
                    }

                    // Perform repo sync
                    send_log!(LogLevel::Info, "Performing 'repo sync'...".to_string());
                    let mut repo_sync_command = Command::new("repo");
                    repo_sync_command.args([
                            "sync",
                            "-c",
                            "--force-sync",
                            "--no-clone-bundle",
                            "--no-tags",
                            format!("-j{}", parallel_jobs.to_string()).as_str(),
                        ])
                        .current_dir(&build_dir_clone);
                    if force_checkout {
                        repo_sync_command.arg("--force-remove-dirty");
                    }
                    Self::configure_repo_command_env(
                        &mut repo_sync_command,
                        &build_dir_clone,
                        askpass_path_clone.as_deref(),
                    );
                    let error_file = (&tempdir_clone).join(format!("{}-{}", "repo-sync", &build_log_filename_suffix));
                    info!("Repo sync output log path: {:?}", &error_file);

                    let repo_sync_status = runner.run_command_with_logs(
                        repo_sync_command,
                        &log_tx_clone,
                        Some(&mut kill_rx),
                        Some(error_file.clone()),
                        None,
                    ).await?;
                    if !repo_sync_status {
                        send_log!(LogLevel::Info, "'repo sync' command failed or was cancelled.".to_string());
                        // Update known builds entry to contain failure
                        let known_builds_self = &mut known_builds_clone.lock().await;
                        if let Some(build_entry) = known_builds_self.iter_mut().find(|b| b.id == build_id_clone) {
                            let content = fs.read_to_string(&error_file).unwrap_or_else(|_| "Failed to read error log.".to_string());
                            // Remove error log file after reading
                            fs.remove_file(&error_file).unwrap_or_else(|e| {
                                error!("Failed to remove error log file {:?}: {}", &error_file, e);
                            });
                            build_entry.success = BuildStatus::Failed(content);
                        }
                        return Err(tonic::Status::internal("'repo sync' command failed."));
                    }
                    send_log!(LogLevel::Info, "'repo sync' completed successfully.".to_string());
                };

                #[cfg(unix)]
                {
                    // Get current nofile limits
                    use nix::sys::resource::{getrlimit, Resource};
                    let (soft_limit, hard_limit) = getrlimit(
                        Resource::RLIMIT_NOFILE,
                    ).map_err(|e| {
                        tonic::Status::internal(format!("Failed to get nofile limit: {}", e))
                    })?;

                    send_log!(LogLevel::Info, format!("Current nofile limits - soft: {}, hard: {}", soft_limit, hard_limit));

                    // AOSP requires us to have at least 16000, but why not 65536?
                    let aosp_soft_limit = 65536;
                    let new_hard_limit = std::cmp::max(hard_limit, aosp_soft_limit);
                    let new_soft_limit = std::cmp::max(aosp_soft_limit, soft_limit);
                    if new_soft_limit == soft_limit && new_hard_limit == hard_limit {
                        send_log!(LogLevel::Info, "Current nofile limits meet AOSP requirements, no need to change.".to_string());
                    } else {
                        send_log!(LogLevel::Info, format!("Setting nofile limits - soft: {}, hard: {}", new_soft_limit, new_hard_limit));
                        let rlim = rlimit {
                            rlim_cur: new_soft_limit,
                            rlim_max: new_hard_limit,
                        };
                        unsafe {
                            use nix::sys::resource::Resource;

                            if setrlimit(Resource::RLIMIT_NOFILE as u32, &rlim) != 0 {
                                return Err(tonic::Status::internal("Failed to set nofile limit."));
                            }
                        }
                        send_log!(LogLevel::Info, "Successfully set nofile limits.".to_string());
                    }
                }

                // Now, start the build process
                send_log!(LogLevel::Info, "Starting build process...".to_string());

                let use_ccache = build_settings.use_ccache.unwrap_or(false);
                let use_rbe = build_settings.use_rbe_service.unwrap_or(false);

                if use_rbe {
                    send_log!(LogLevel::Info, "Writing RBE environment configuration...".to_string());
                    Self::setup_rbe_env(
                        fs.as_ref(),
                        build_dir_clone.clone(),
                        build_settings.rbe_api_token.as_deref().unwrap_or(""),
                    ).await.map_err(|e| {
                        tonic::Status::internal(format!("Failed to write RBE environment configuration: {}", e))
                    })?;
                    send_log!(LogLevel::Info, "RBE environment configuration written successfully.".to_string());
                }

                // Detect vendor type.
                let vendor_dir = build_dir_clone.join("vendor");
                // Check vendor/<vendor>/config/BoardConfigSoong.mk for known vendors
                let mut vendor_name : String = "unknown".to_string();
                for dir in fs.read_dir(&vendor_dir).map_err(|e| {
                    tonic::Status::internal(format!("Failed to read vendor directory: {}", e))
                })? {
                    // Check for config/BoardConfigSoong.mk. This requires Android 10+, but most ROMs now target that or higher.
                    // Android 9 is released on August 2018, (that was 8 years ago) so it's reasonable to assume most builds are Android 10+ now.
                    // One exception may be TWRP, but you cannot build old versions of TWRP with this system anyway. (Need docker images with old toolchains)
                    let config_path = dir.join("config").join("BoardConfigSoong.mk");
                    if fs.exists(&config_path) {
                        vendor_name = dir
                            .file_name()
                            .unwrap_or_default()
                            .to_string_lossy()
                            .to_string();
                        send_log!(LogLevel::Info, format!("Detected vendor: {}", vendor_name));
                        break;
                    }
                }
                if vendor_name == "unknown" {
                    send_log!(LogLevel::Warning, "Could not detect vendor from vendor directory.".to_string());
                    vendor_name = "lineage".to_string(); // Default to lineage
                    send_log!(LogLevel::Info, format!("Defaulting to vendor: {} for build.", vendor_name));
                } else {
                    send_log!(LogLevel::Info, format!("Using vendor: {} for build.", vendor_name));
                }

                // Detect build release
                let mut release: Option<String> = None;
                // We have two places to look for build release: build/release and vendor/<vendor>/build/release.
                for path in [&build_dir_clone.join("build").join("release"), &build_dir_clone.join("vendor").join(&vendor_name).join("build").join("release")] {
                    // First, check release_config_map.textproto. Refer: https://android.googlesource.com/platform/build/release/+/925b392ae2a5d212904adec6cfebf4d1b8f574d9.
                    let release_config_map_path = path.join("release_config_map.textproto");
                    if fs.exists(&release_config_map_path) {
                        // In this case, aosp_current is automatically mapped to the latest release.
                        send_log!(LogLevel::Info, format!("Detected release from release_config_map.textproto"));
                        release = Some("aosp_current".to_string());
                        break;
                    }

                    // Check build/release/build_config, scl for Android 14
                    let _build_config_path = path.join("build_config");
                    if fs.is_dir(&_build_config_path) {
                        for file in fs.read_dir(&path).map_err(|e| {
                            tonic::Status::internal(format!("Failed to read release directory: {}", e))
                        })? {
                            if file.extension().and_then(|s| s.to_str()) == Some("scl") {
                                let file_name = file.file_name().unwrap_or_default().to_string_lossy().to_string();
                                let release_name = file_name.trim_end_matches(".scl");
                                send_log!(LogLevel::Info, format!("Detected release from build_config scl file: {}", release_name));
                                release = Some(release_name.to_string());
                                break; // Use the first valid one
                            }
                        }
                    }

                    // Fallback to build/release/release_configs, textproto, another stuff added on android 15
                    let release_configs_path = path.join("release_configs");
                    if fs.is_dir(&release_configs_path) {
                        let _latest_release: Option<String> = None;
                        for file in fs.read_dir(&release_configs_path).map_err(|e| {
                            tonic::Status::internal(format!("Failed to read release_configs directory: {}", e))
                        })? {
                            if file.extension().and_then(|s| s.to_str()) == Some("textproto") {
                                let file_name = file.file_name().unwrap_or_default().to_string_lossy().to_string();
                                let release_name = file_name.trim_end_matches(".textproto");
                                if vec!["root", "trunk"].contains(&release_name) {
                                    continue;
                                }
                                send_log!(LogLevel::Info, format!("Detected release from release_configs textproto file: {}", release_name));
                                release = Some(release_name.to_string());
                                break; // Use the first valid one
                            }
                        }
                    }
                };
                if release.is_none() {
                    send_log!(LogLevel::Warning, "Could not detect release from build/release directory.".to_string());
                } else {
                    send_log!(LogLevel::Info, format!("Using release: {} for build.", release.as_ref().unwrap()));
                }

                let build_variant  = match req.build_variant {
                    val if val == BuildVariant::User as i32 => "user",
                    val if val == BuildVariant::UserDebug as i32 => "userdebug",
                    val if val == BuildVariant::Eng as i32 => "eng",
                    _ => {
                        return Err(tonic::Status::invalid_argument(
                            "Invalid build variant specified.",
                        ))
                    }
                };

                if build_settings.do_clean_build() {
                    send_log!(LogLevel::Info, "Performing clean build...".to_string());
                    let out_dir = build_dir_clone.join("out");
                    if fs.exists(&out_dir) {
                        send_log!(LogLevel::Info, format!("Removing output directory at {:?}", out_dir));
                        fs.remove_dir_all(&out_dir).map_err(|e| {
                            tonic::Status::internal(format!(
                                "Failed to remove output directory for clean build: {}",
                                e
                            ))
                        })?;
                    }
                }

                // Delete matching artifact from previous builds to prevent confusion. We will check for the artifact after the build completes, 
                // if it's not there, we know the build failed. If it's there, we know the build succeeded and we can proceed to upload.
                match Self::find_artifact(fs.as_ref(), &build_dir_clone, &device_entry.codename, &rom_entry).await {
                    Ok(artifacts) => {
                        send_log!(LogLevel::Info, format!("Removing existing artifact from previous builds to prevent confusion. (count: {})", artifacts.len()));
                        for artifact in artifacts {
                            fs.remove_file(&artifact).map_err(|e| {
                                tonic::Status::internal(format!(
                                    "Failed to remove existing artifact for clean build: {}",
                                    e
                                ))
                            })?;
                        }
                    }
                    Err(e) => {
                        send_log!(LogLevel::Warning, format!("Failed to check for existing artifacts before build: {}", e));
                    }
                }

                let mut cmd = Command::new("bash");
                cmd.current_dir(&build_dir_clone);
                Self::configure_repo_command_env(
                    &mut cmd,
                    &build_dir_clone,
                    askpass_path_clone.as_deref(),
                );

                let (stdin_tx, stdin_rx) = mpsc::channel(100);

                let mut command_list = Vec::new();
                command_list.push("set -e".to_string());
                if use_rbe {
                    command_list.push(format!("source {}", build_dir_clone.join("rbe_env.sh").to_string_lossy()));
                }
                if !use_ccache {
                    command_list.push("unset USE_CCACHE; unset CCACHE_EXEC;".to_string());
                }
                command_list.push("source build/envsetup.sh".to_string());

                // These values originate from on-disk config and repo-synced
                // filenames, so shell-quote every interpolated component before
                // feeding it to bash. Adjacent quoted/unquoted tokens still
                // concatenate into the expected lunch combo argument.
                let command = match release {
                    Some(ref rel) => {
                        format!("lunch {}_{}-{}-{}",
                            Self::shell_single_quote(vendor_name.as_str()),
                            Self::shell_single_quote(device_entry.codename.as_str()),
                            Self::shell_single_quote(rel.as_str()),
                            Self::shell_single_quote(build_variant))
                    }
                    None => {
                        format!("lunch {}_{}-{}",
                            Self::shell_single_quote(vendor_name.as_str()),
                            Self::shell_single_quote(device_entry.codename.as_str()),
                            Self::shell_single_quote(build_variant))
                    }
                };
                command_list.push(command);
                command_list.push(format!("m {} -j{}",
                    Self::shell_single_quote(rom_entry.target.as_str()),
                    parallel_jobs));
                command_list.push("exit 0".to_string());

                for line in command_list {
                    stdin_tx.send((&line).clone().into()).await.map_err(|e| tonic::Status::internal(format!("Failed to send to stdin: {}", e)))?;
                    send_log!(LogLevel::Info, format!("Sent to stdin: {}", line));
                }

                let error_file_path = (&tempdir_clone).join(format!("{}-{}", "build-output", &build_log_filename_suffix));
                if !runner.run_command_with_logs(cmd,
                    &log_tx_clone,
                    Some(&mut kill_rx),
                    Some(error_file_path.clone()),
                    stdin_rx.into(),
                ).await? {
                    send_log!(LogLevel::Error, "Build command failed".to_string());
                    // Update known builds entry to contain failure
                    let known_builds_self = &mut known_builds_clone.lock().await;
                    if let Some(build_entry) = known_builds_self.iter_mut().find(|b| b.id == build_id_clone) {
                        let error_file = (&build_dir_clone).join("out").join("error.log");
                        send_log!(LogLevel::Info, format!("Reading error log from {:?}", error_file));
                        let content = fs.read_to_string(&error_file).unwrap_or_else(|_| "Failed to read error log.".to_string());
                        if content.is_empty() {
                            send_log!(LogLevel::Info, format!("Error log is empty, reading from build output log at {:?}", error_file_path));
                            let content = fs.read_to_string(&error_file_path).unwrap_or_else(|_| "Failed to read error log.".to_string());
                            if content.is_empty() {
                                send_log!(LogLevel::Warning, "Build output log is also empty, setting generic error message.".to_string());
                                build_entry.success = BuildStatus::Failed("Build failed, but no error log available.".to_string());
                            } else {
                                build_entry.success = BuildStatus::Failed(content);
                            }
                        } else {
                            build_entry.success = BuildStatus::Failed(content);
                        }
                    }
                    return Err(tonic::Status::internal("Build command failed or was cancelled."));
                }
                send_log!(LogLevel::Info, "Build process completed successfully.".to_string());

                if build_settings.do_upload() {
                    match Self::find_artifact(fs.as_ref(), &build_dir_clone, &device_entry.codename, &rom_entry).await {
                        Ok(artifact) => {
                        send_log!(LogLevel::Info, format!("Found {} artifact(s) for upload.", artifact.len()));
                        for (i, path) in artifact.iter().enumerate() {
                            send_log!(LogLevel::Info, format!("Artifact {}: {:?}", i + 1, path));
                        }
                        if artifact.len() != 1 {
                            send_log!(LogLevel::Warning, format!("Expected to find exactly one artifact, but found {}.", artifact.len()));
                        }
                        let artifact_it = artifact.into_iter().next().unwrap_or_else(|| {
                            send_log!(LogLevel::Error, "Failed to select an artifact for upload.".to_string());
                            std::path::PathBuf::new()
                        });
                        send_log!(LogLevel::Info, format!("Build artifact located at: {:?}", artifact_it));
                        let mut uploads = uploads_clone.lock().await;
                        match req.upload_method {
                            val if val == UploadMethod::None as i32 => {
                                send_log!(LogLevel::Info, "Upload method set to None, skipping upload.".to_string());
                            }
                            val if val == UploadMethod::LocalFile as i32 => {
                                send_log!(LogLevel::Info, format!("File ready at local path: {:?}", artifact_it));
                                uploads.push(
                                    UploadTask {
                                        build_id: build_id_clone.clone(),
                                        method: UploadMethod::LocalFile,
                                        artifact_path: artifact_it.clone(),
                                    }
                                );
                            }
                            val if val == UploadMethod::GoFile as i32 => {
                                send_log!(LogLevel::Info, "Scheduling upload to GoFile...".to_string());
                                uploads.push(
                                    UploadTask {
                                        build_id: build_id_clone.clone(),
                                        method: UploadMethod::GoFile,
                                        artifact_path: artifact_it.clone(),
                                    }
                                );
                            }
                            val if val == UploadMethod::Stream as i32 => {
                                send_log!(LogLevel::Info, "Scheduling upload to Stream...".to_string());
                                uploads.push(
                                    UploadTask {
                                        build_id: build_id_clone.clone(),
                                        method: UploadMethod::Stream,
                                        artifact_path: artifact_it.clone(),
                                    }
                                );
                            }
                            _ => {
                                send_log!(LogLevel::Warning, "Unknown upload method specified, skipping upload.".to_string());   
                            }
                        }

                        }
                        Err(e) => {
                            send_log!(LogLevel::Error, format!("Failed to find build artifact: {}", e));
                        }
                    }
                } else {
                    send_log!(LogLevel::Info, "Upload disabled in settings, skipping artifact search.".to_string());
                };

                // Mark build as successful in known builds
                let known_builds_self = &mut known_builds_clone.lock().await;
                if let Some(build_entry) = known_builds_self.iter_mut().find(|b| b.id == build_id_clone) {
                    build_entry.success = BuildStatus::Success;
                }
                Ok(())
            }.instrument(span);

            match res.await {
                Ok(_) => {
                    send_log_final!(
                        LogLevel::Info,
                        String::from("Build completed successfully.")
                    );
                }
                Err(e) => {
                    // Update known builds entry to contain failure
                    let known_builds_self = &mut known_builds_clone.lock().await;
                    if let Some(build_entry) = known_builds_self
                        .iter_mut()
                        .find(|b| b.id == build_id_clone)
                    {
                        if let BuildStatus::Failed(msg) = &build_entry.success {
                            // Already have an error message, do not overwrite. Just append.
                            build_entry.success = BuildStatus::Failed(format!(
                                "{}\nWhich caused builder error: {}",
                                msg, e
                            ));
                        } else {
                            build_entry.success = BuildStatus::Failed(format!("{}", e));
                        }
                    }
                    send_log_final!(LogLevel::Error, format!("Build failed: {}", e));
                }
            }

            // Cleanup when done
            let mut lock = active_job_cleanup.lock().await;
            *lock = None;

            Ok(())
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
            settings: Arc::new(Mutex::new(Settings {
                do_repo_sync: Some(true),
                do_clean_build: Some(false),
                use_ccache: Some(false),
                use_rbe_service: Some(false),
                rbe_api_token: None,
                do_upload: Some(false),
            })),
            build_dir,
            tempdir: temp_dir,
            configs,
            active_job: Arc::new(Mutex::new(None)),
            active_uploads: Arc::new(Mutex::new(Vec::new())),
            known_builds: Arc::new(Mutex::new(Vec::new())),
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
        async fn run_command_with_logs(
            &self,
            command: Command,
            _tx: &broadcast::Sender<BuildLogEntry>,
            _kill_rx: Option<&mut mpsc::Receiver<()>>,
            _log_path: Option<PathBuf>,
            mut stdin_rx: Option<mpsc::Receiver<String>>,
        ) -> Result<bool, Status> {
            self.calls.lock().await.push(Self::argv(&command));
            // Drain queued stdin so the workflow's senders don't block/observe a
            // closed channel.
            if let Some(rx) = stdin_rx.as_mut() {
                while rx.try_recv().is_ok() {}
            }
            Ok(self.results.lock().await.pop_front().unwrap_or(true))
        }
    }

    #[tokio::test]
    async fn mock_records_argv_and_returns_scripted_result() {
        let runner = MockProcessRunner::default();
        runner.results.lock().await.push_back(false);

        let (tx, _rx) = broadcast::channel(16);
        let mut cmd = Command::new("repo");
        cmd.arg("sync").arg("-j8");

        let ok = runner
            .run_command_with_logs(cmd, &tx, None, None, None)
            .await
            .unwrap();

        assert!(!ok, "scripted failure should propagate");
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
    use crate::filesystem::mock::MockFilesystem;
    use crate::git_repo::mock::MockGitProvider;
    use crate::rombuild::types::ROMArtifactEntry;
    use std::collections::VecDeque;

    fn task_with(
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
            // do_repo_sync=false skips the manifest/repo phase, so the pipeline
            // goes straight to the build command — the path under test.
            build_settings: Settings {
                do_repo_sync: Some(false),
                do_clean_build: Some(false),
                use_ccache: Some(false),
                use_rbe_service: Some(false),
                rbe_api_token: None,
                do_upload: Some(false),
            },
            build_dir_clone: PathBuf::from("/build"),
            log_tx_clone: broadcast::channel(100).0,
            uploads_clone: Arc::new(Mutex::new(Vec::new())),
            build_id_clone: "build-test".into(),
            tempdir_clone: PathBuf::from("/tmp"),
            known_builds_clone: known_builds,
            askpass_path_clone: None,
            runner,
            git,
            fs,
            force_checkout: false,
            parallel_jobs: 1,
            req: BuildRequest::default(),
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
            active_job_cleanup: Arc::new(Mutex::new(None)),
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
            variant: BuildVariant::User,
            target_device: TargetsEntry {
                name: "OnePlus 6".into(),
                codename: "enchilada".into(),
                manufacturer: "oneplus".into(),
            },
            config_name: "cfg".into(),
            success: BuildStatus::InProgress,
        }]));

        let task = task_with(runner.clone(), git, fs, known_builds.clone());

        // run_build is the spawned task's body, so it always resolves to Ok(());
        // a failure is surfaced by marking the build Failed and ending the
        // pipeline (no upload), not by an Err return.
        let result = BuildService::run_build(task).await;
        assert!(result.is_ok(), "the task wrapper resolves Ok after handling");

        // The build command was actually reached and run...
        let calls = runner.calls.lock().await;
        assert_eq!(calls.len(), 1, "exactly the build command should have run");
        assert_eq!(calls[0][0], "bash", "the build command is the bash invocation");
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
            variant: BuildVariant::User,
            target_device: TargetsEntry {
                name: "OnePlus 6".into(),
                codename: "enchilada".into(),
                manufacturer: "oneplus".into(),
            },
            config_name: "cfg".into(),
            success: BuildStatus::InProgress,
        }]));

        let task = task_with(runner.clone(), git, fs, known_builds.clone());
        BuildService::run_build(task).await.unwrap();

        assert_eq!(runner.calls.lock().await.len(), 1);
        let kb = known_builds.lock().await;
        assert!(
            matches!(kb[0].success, BuildStatus::Success),
            "a successful build command must mark the build Success"
        );
    }
}
