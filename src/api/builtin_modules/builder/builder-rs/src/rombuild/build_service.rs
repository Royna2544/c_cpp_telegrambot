mod grpc_pb {
    include!(concat!(env!("OUT_DIR"), "/tgbot.builder.android.rs"));
}

pub use crate::rombuild::build_service::grpc_pb::rom_build_service_server::RomBuildServiceServer;
use crate::{
    git_repo::{self, GitRepo},
    gofile_api::upload_file_to_gofile,
    ratelimit::RateLimit,
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
    num::NonZero,
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
use tracing_subscriber::field::debug;
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

struct BuildEntry {
    id: String,
    variant: BuildVariant,
    target_device: TargetsEntry,
    config_name: String,
    success: bool,
    error_message: Option<String>, // Optional error message if the build failed
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

pub struct BuildService {
    settings: Arc<Mutex<Settings>>,
    build_dir: PathBuf,
    tempdir: PathBuf,
    configs: ROMBuildConfig,
    active_job: Arc<Mutex<Option<ActiveBuild>>>,
    active_uploads: Arc<Mutex<Vec<UploadTask>>>,
    known_builds: Arc<Mutex<Vec<BuildEntry>>>,
}

impl BuildService {
    pub fn new(build_dir: PathBuf, temp_dir: PathBuf, configs: ROMBuildConfig) -> Self {
        let settings = Settings {
            do_repo_sync: Some(true),
            do_clean_build: Some(false),
            use_ccache: Some(false),
            use_rbe_service: Some(false),
            rbe_api_token: None,
            do_upload: Some(false),
        };
        BuildService {
            settings: Arc::new(Mutex::new(settings)),
            build_dir,
            tempdir: temp_dir,
            configs,
            active_job: Arc::new(Mutex::new(None)),
            active_uploads: Arc::new(Mutex::new(Vec::new())),
            known_builds: Arc::new(Mutex::new(Vec::new())),
        }
    }

    async fn run_command_with_logs(
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
        std::fs::remove_dir_all(&clean_dir)
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
        let exists = check_dir.exists();
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
        let (log_tx, _) = broadcast::channel::<BuildLogEntry>(1000);

        let log_tx_clone_for_ctrlc = log_tx.clone();
        let kill_tx_clone_for_ctrlc = kill_tx.clone();

        // Spawn an async task to listen for Ctrl-C just for the duration of this build
        tokio::spawn(async move {
            tokio::select! {
                // Option 1: The user presses Ctrl-C
                _ = tokio::signal::ctrl_c() => {
                    let _ = kill_tx_clone_for_ctrlc.try_send(());
                    let _ = log_tx_clone_for_ctrlc.send(BuildLogEntry {
                        level: LogLevel::Warning as i32,
                        message: "Cancellation requested via Ctrl-C.".into(),
                        timestamp: chrono::Utc::now().timestamp(),
                        is_finished: false,
                    });
                    tracing::info!("Build cancelled via Ctrl-C signal.");
                }
                // Option 2: The build finishes and kill_rx is dropped
                _ = kill_tx_clone_for_ctrlc.closed() => {
                    // The receiver no longer exists, meaning the build task is done.
                    // This task exits cleanly without leaking memory.
                }
            }
        });

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

        enum ConfigType {
            Standard(ManifestEntry),
            Recovery(RecoveryManifestEntry),
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
            success: false,
            error_message: None,
        };
        self.known_builds.lock().await.push(known_builds_entry);

        // Write git-askpass file if github token is provided
        if let Some(token) = &req.github_token {
            let askpass_path = self.build_dir.join("git-askpass.sh");
            info!("Writing git-askpass file to {:?}", askpass_path);
            let res =
                tokio::fs::write(&askpass_path, format!("#!/bin/sh\necho \"{}\"\n", token)).await;
            if let Err(e) = res {
                return Err(Status::internal(format!(
                    "Failed to write git-askpass file: {}",
                    e
                )));
            }
            // Make it executable
            #[cfg(unix)]
            {
                use std::os::unix::fs::PermissionsExt;
                let mut perms = std::fs::metadata(&askpass_path)
                    .inspect_err(|e| info!("Failed to get metadata for git-askpass file: {}", e))?
                    .permissions();
                perms.set_mode(0o700);
                std::fs::set_permissions(&askpass_path, perms).map_err(|e| {
                    tonic::Status::internal(format!(
                        "Failed to set permissions for git-askpass file: {}",
                        e
                    ))
                })?;
            }
            // Set GIT_ASKPASS environment
            // SAFETY: This is safe because we are only using single thread, let me fix it when I change.
            unsafe {
                std::env::set_var("GIT_ASKPASS", askpass_path.to_str().unwrap());
            }
        }

        let settings_clone = self.settings.clone();
        let build_dir_clone = self.build_dir.clone();
        let log_tx_clone = log_tx.clone();
        let uploads_clone = self.active_uploads.clone();
        let build_id_clone = build_id.clone();
        let tempdir_clone = self.tempdir.clone();
        let known_builds_clone = self.known_builds.clone();
        let force_checkout = req.force_checkout.unwrap_or(false);

        let span = tracing::info_span!("build_task", build_id = build_id);
        let task_handle: tokio::task::JoinHandle<Result<(), Status>> = tokio::spawn(async move {
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
                if settings_clone.lock().await.do_repo_sync.unwrap() {
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
                    if manifest_repo_path.exists() {
                        send_log!(LogLevel::Debug, format!("Opening manifest git repository at {:?}", manifest_repo_path));
                        let repo = git_repo::GitRepo::new(&manifest_repo_path, "origin", None, None)
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

                        let (stdin_tx, stdin_rx) = mpsc::channel(10);

                        // Sometimes, repo init may ask to "enable colored output" --- we auto-confirm it.
                        stdin_tx
                            .send("y".into())
                            .await
                            .map_err(|e| tonic::Status::internal(format!("Failed to send to stdin: {}", e)))?;

                        let error_file = (&tempdir_clone).join(format!("{}-{}", "repo-init", &build_log_filename_suffix));
                        info!("Repo init output log path: {:?}", &error_file);

                        let res = Self::run_command_with_logs(
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
                                let content = std::fs::read_to_string(&error_file).unwrap_or_else(|_| "Failed to read error log.".to_string());
                                // Remove error log file after reading
                                std::fs::remove_file(&error_file).unwrap_or_else(|e| {
                                    error!("Failed to remove error log file {:?}: {}", &error_file, e);
                                });
                                build_entry.error_message = Some(content);
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
                            match git_repo::GitRepo::new(
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
                                        std::fs::remove_dir_all(&local_manifest_dir).map_err(|e| {
                                        tonic::Status::internal(format!(
                                            "Failed to remove existing local manifest directory: {}",
                                            e
                                        ))
                                    })?;
                                        if force_checkout {
                                            send_log!(LogLevel::Info, "Force checkout enabled, removing local manifest directory before cloning.".to_string());
                                            std::fs::remove_dir_all(&local_manifest_dir).map_err(|e| {
                                                tonic::Status::internal(format!(
                                                    "Failed to remove local manifest directory: {}",
                                                    e
                                                ))
                                            })?;
                                        }

                                        git_repo::GitRepo::clone(
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
                                    git_repo::GitRepo::clone(
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
                            for file in std::fs::read_dir(&local_manifest_dir).map_err(|e| {
                                tonic::Status::internal(format!(
                                    "Failed to read local manifests directory: {}",
                                    e
                                ))
                            })?
                            {
                                let path = file.map_err(|e| {
                                    tonic::Status::internal(format!(
                                        "Failed to read local manifest file entry: {}",
                                        e
                                    ))
                                })?.path();
                                if path.extension().and_then(|s| s.to_str()) == Some("xml") {
                                    // Parse XML to check for recurse_submodules attribute
                                    let content = std::fs::read_to_string(&path).map_err(|e| {
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

                                                        let sub_repo_path = attributes.iter()
                                                                .find(|attr| attr.name.local_name == "name").map(|attr| attr.value.clone());
                                                        if sub_repo_path.is_none() {
                                                            send_log!(LogLevel::Warning, format!("recurse_submodules=true is set but no project name found in manifest file {:?}, skipping submodule update.", path));
                                                            continue;
                                                        }

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
                                                                &(&sub_repo_path.clone()),
                                                            ])
                                                            .current_dir(&build_dir_clone);
                                                        let error_file = (&tempdir_clone).join(format!("{}-{}-submodule-sync.log", "repo-sync", &build_id_clone));
                                                        info!("Repo sync for submodule output log path: {:?}", &error_file);
                                                        let repo_sync_status = Self::run_command_with_logs(
                                                            repo_sync_command,
                                                            &log_tx_clone,
                                                            Some(&mut kill_rx),
                                                            Some(error_file.clone()),
                                                            None,
                                                        ).await?;
                                                        if !repo_sync_status {
                                                            send_log!(LogLevel::Warning, format!("'repo sync' for submodule {} failed or was cancelled.", &sub_repo_path));
                                                            continue;
                                                        }

                                                        let sub_repo = GitRepo::new(
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
                            if !local_manifest_dir.exists() {
                                std::fs::create_dir_all(&local_manifest_dir).map_err(|e| {
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
                    let error_file = (&tempdir_clone).join(format!("{}-{}", "repo-sync", &build_log_filename_suffix));
                    info!("Repo sync output log path: {:?}", &error_file);

                    let repo_sync_status = Self::run_command_with_logs(
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
                            let content = std::fs::read_to_string(&error_file).unwrap_or_else(|_| "Failed to read error log.".to_string());
                            // Remove error log file after reading
                            std::fs::remove_file(&error_file).unwrap_or_else(|e| {
                                error!("Failed to remove error log file {:?}: {}", &error_file, e);
                            });
                            build_entry.error_message = Some(content);
                        }
                        return Err(tonic::Status::internal("'repo sync' command failed."));
                    }
                    send_log!(LogLevel::Info, "'repo sync' completed successfully.".to_string());
                };

                #[cfg(unix)]
                {
                    // Get current nofile limits
                    use nix::libc;
                    use nix::sys::resource::{getrlimit, Resource};
                    let (soft_limit, hard_limit) : (libc::rlim_t, libc::rlim_t);
                    (soft_limit, hard_limit) = getrlimit(
                        Resource::RLIMIT_NOFILE,
                    ).map_err(|e| {
                        tonic::Status::internal(format!("Failed to get nofile limit: {}", e))
                    })?;

                    send_log!(LogLevel::Info, format!("Current nofile limits - soft: {}, hard: {}", soft_limit, hard_limit));

                    // AOSP requires us to have at least 16000, but why not 65536?
                    let new_soft_limit = 65536;
                    let new_hard_limit = if hard_limit > new_soft_limit {
                        hard_limit
                    } else {
                        new_soft_limit
                    };
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

                // Now, start the build process
                send_log!(LogLevel::Info, "Starting build process...".to_string());
                let no_ccache = if !settings_clone.lock().await.use_ccache.unwrap() {
                    "unset USE_CCACHE; unset CCACHE_EXEC;"
                } else {
                    "true"
                };

                // Detect vendor type.
                let vendor_dir = build_dir_clone.join("vendor");
                // Check vendor/<vendor>/config/BoardConfigSoong.mk for known vendors
                let mut vendor_name : String = "unknown".to_string();
                for dir in std::fs::read_dir(&vendor_dir).map_err(|e| {
                    tonic::Status::internal(format!("Failed to read vendor directory: {}", e))
                })? {
                    let dir = dir.map_err(|e| {
                        tonic::Status::internal(format!("Failed to read vendor subdirectory: {}", e))
                    })?;
                    // Check for config/BoardConfigSoong.mk. This requires Android 10+, but most ROMs now target that or higher.
                    // Android 9 is released on August 2018, (that was 8 years ago) so it's reasonable to assume most builds are Android 10+ now.
                    // One exception may be TWRP, but you cannot build old versions of TWRP with this system anyway. (Need docker images with old toolchains)
                    let config_path = dir.path().join("config").join("BoardConfigSoong.mk");
                    if config_path.exists() {
                        vendor_name = dir.file_name().to_string_lossy().to_string();
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
                    if release_config_map_path.exists() {
                        // In this case, aosp_current is automatically mapped to the latest release.
                        send_log!(LogLevel::Info, format!("Detected release from release_config_map.textproto"));
                        release = Some("aosp_current".to_string());
                        break;
                    }

                    // Check build/release/build_config, scl for Android 14
                    let _build_config_path = path.join("build_config");
                    if _build_config_path.is_dir() {
                        for file in std::fs::read_dir(&path).map_err(|e| {
                            tonic::Status::internal(format!("Failed to read release directory: {}", e))
                        })? {
                            let file = file.map_err(|e| {
                                tonic::Status::internal(format!("Failed to read release file entry: {}", e))
                            })?;
                            if file.path().extension().and_then(|s| s.to_str()) == Some("scl") {
                                let file_name = file.file_name().to_string_lossy().to_string();
                                let release_name = file_name.trim_end_matches(".scl");
                                send_log!(LogLevel::Info, format!("Detected release from build_config scl file: {}", release_name));
                                release = Some(release_name.to_string());
                                break; // Use the first valid one
                            }
                        }
                    }

                    // Fallback to build/release/release_configs, textproto, another stuff added on android 15
                    let release_configs_path = path.join("release_configs");
                    if release_configs_path.is_dir() {
                        let _latest_release: Option<String> = None;
                        for file in std::fs::read_dir(&release_configs_path).map_err(|e| {
                            tonic::Status::internal(format!("Failed to read release_configs directory: {}", e))
                        })? {
                            let file = file.map_err(|e| {
                                tonic::Status::internal(format!("Failed to read release_configs file entry: {}", e))
                            })?;
                            if file.path().extension().and_then(|s| s.to_str()) == Some("textproto") {
                                let file_name = file.file_name().to_string_lossy().to_string();
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

                if settings_clone.lock().await.do_clean_build() {
                    send_log!(LogLevel::Info, "Performing clean build...".to_string());
                    let out_dir = build_dir_clone.join("out");
                    if out_dir.exists() {
                        send_log!(LogLevel::Info, format!("Removing output directory at {:?}", out_dir));
                        std::fs::remove_dir_all(&out_dir).map_err(|e| {
                            tonic::Status::internal(format!(
                                "Failed to remove output directory for clean build: {}",
                                e
                            ))
                        })?;
                    }
                }

                let mut cmd = Command::new("bash");
                cmd.current_dir(&build_dir_clone);

                let (stdin_tx, stdin_rx) = mpsc::channel(100);

                let command = match release {
                    Some(ref rel) => {
                        format!("set -e\nsource build/envsetup.sh\n{}\nlunch {}_{}-{}-{}\nm {} -j{}\nexit 0\n", 
                            no_ccache, vendor_name, device_entry.codename, rel, build_variant, rom_entry.target, parallel_jobs)
                    }
                    None => {
                        format!("set -e\nsource build/envsetup.sh\n{}\nllunch {}_{}-{}\nm {} -j{}\nexit 0\n", 
                            no_ccache, vendor_name, device_entry.codename, build_variant, rom_entry.target, parallel_jobs)
                    }
                };

                for line in command.lines() {
                    stdin_tx.send(line.into()).await.map_err(|e| tonic::Status::internal(format!("Failed to send to stdin: {}", e)))?;
                    send_log!(LogLevel::Info, format!("Sent to stdin: {}", line));
                }

                let error_file_path = (&tempdir_clone).join(format!("{}-{}", "build-output", &build_log_filename_suffix));
                if !Self::run_command_with_logs(cmd,
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
                        let content = std::fs::read_to_string(&error_file).unwrap_or_else(|_| "Failed to read error log.".to_string());
                        if content.is_empty() {
                            send_log!(LogLevel::Info, format!("Error log is empty, reading from build output log at {:?}", error_file_path));
                            let content = std::fs::read_to_string(&error_file_path).unwrap_or_else(|_| "Failed to read error log.".to_string());
                            if content.is_empty() {
                                send_log!(LogLevel::Warning, "Build output log is also empty, setting generic error message.".to_string());
                                build_entry.error_message = Some("Build failed, but no error log available.".to_string());
                            } else {
                                build_entry.error_message = Some(content);
                            }
                        } else {
                            build_entry.error_message = Some(content);
                        }
                    }
                    return Err(tonic::Status::internal("Build command failed or was cancelled."));
                }
                send_log!(LogLevel::Info, "Build process completed successfully.".to_string());

                if settings_clone.lock().await.do_upload() {
                    // Finally, check for output file
                    let output_dir = build_dir_clone.join("out").join("target").join("product").join(&device_entry.codename);
                    let artifact = match rom_entry.artifact.matcher {
                        ROMArtifactMatcher::ZipFilePrefixer => {
                            let prefix = rom_entry.artifact.data;
                            let mut artifact_path: Option<PathBuf> = None;
                            let mut found = false;
                            for entry in std::fs::read_dir(&output_dir).map_err(|e| {
                                tonic::Status::internal(format!("Failed to read output directory: {}", e))
                            })? {
                                let entry = entry.map_err(|e| {
                                    tonic::Status::internal(format!("Failed to read output entry: {}", e))
                                })?;
                                let file_name = entry.file_name().to_string_lossy().to_string();
                                if file_name.starts_with(&prefix) && file_name.ends_with(".zip") {
                                    send_log!(LogLevel::Info, format!("Found output artifact: {}", file_name));
                                    artifact_path = Some(output_dir.join(&file_name));
                                    found = true;
                                    break;
                                }
                            }
                            if !found {
                                send_log!(LogLevel::Error, format!("No output artifact found with prefix: {}", prefix));
                                None
                            } else {
                                artifact_path
                            }
                        }
                        ROMArtifactMatcher::ExactMatcher => {
                            let exact_name = rom_entry.artifact.data;
                            let artifact_path = output_dir.join(&exact_name);
                            if !artifact_path.exists() {
                                send_log!(LogLevel::Error, format!("No output artifact found with exact name: {}", exact_name));
                                None
                            } else {
                            send_log!(LogLevel::Info, format!("Found output artifact: {:?}", artifact_path));
                            Some(artifact_path)

                            }
                        }
                    };
                    if let Some(artifact_path) = artifact {
                        send_log!(LogLevel::Info, format!("Build artifact located at: {:?}", artifact_path));
                        let mut uploads = uploads_clone.lock().await;
                        match req.upload_method {
                            val if val == UploadMethod::None as i32 => {
                                send_log!(LogLevel::Info, "Upload method set to None, skipping upload.".to_string());
                            }
                            val if val == UploadMethod::LocalFile as i32 => {
                                send_log!(LogLevel::Info, format!("File ready at local path: {:?}", artifact_path));
                                uploads.push(
                                    UploadTask {
                                        build_id: build_id_clone.clone(),
                                        method: UploadMethod::LocalFile,
                                        artifact_path: artifact_path.clone(),
                                    }
                                );
                            }
                            val if val == UploadMethod::GoFile as i32 => {
                                send_log!(LogLevel::Info, "Scheduling upload to GoFile...".to_string());
                                uploads.push(
                                    UploadTask {
                                        build_id: build_id_clone.clone(),
                                        method: UploadMethod::GoFile,
                                        artifact_path: artifact_path.clone(),
                                    }
                                );
                            }
                            val if val == UploadMethod::Stream as i32 => {
                                send_log!(LogLevel::Info, "Scheduling upload to Stream...".to_string());
                                uploads.push(
                                    UploadTask {
                                        build_id: build_id_clone.clone(),
                                        method: UploadMethod::Stream,
                                        artifact_path: artifact_path.clone(),
                                    }
                                );
                            }
                            _ => {
                                send_log!(LogLevel::Warning, "Unknown upload method specified, skipping upload.".to_string());   
                            }
                        }
                    } else {
                        send_log!(LogLevel::Error, "Build artifact not found.".to_string());
                    }
                } else {
                    send_log!(LogLevel::Info, "Upload disabled in settings, skipping artifact search.".to_string());
                };

                // Mark build as successful in known builds
                let known_builds_self = &mut known_builds_clone.lock().await;
                if let Some(build_entry) = known_builds_self.iter_mut().find(|b| b.id == build_id_clone) {
                    build_entry.success = true;
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
                        if let Some(msg) = &build_entry.error_message {
                            // Already have an error message, do not overwrite. Just append.
                            build_entry.error_message =
                                Some(format!("{}\nWhich caused builder error: {}", msg, e));
                        } else {
                            build_entry.error_message = Some(format!("{}", e));
                        }
                    }
                    send_log_final!(LogLevel::Error, format!("Build failed: {}", e));
                }
            }

            // Cleanup when done
            let mut lock = active_job_cleanup.lock().await;
            *lock = None;

            Ok(())
        });

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

        if !build_entry.success {
            warn!("Entering failure branch for build ID: {}", req.build_id);
            let error_message = build_entry
                .error_message
                .clone()
                .unwrap_or_else(|| "Build failed for unknown reasons.".into());
            tx.send(Ok(BuildResult {
                success: false,
                upload_method: UploadMethod::None as i32,
                result_details: None,
                error_message: Some(error_message),
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

        let upload_tasks = self.active_uploads.lock().await;
        let upload_task = upload_tasks
            .iter()
            .find(|task| task.build_id == req.build_id)
            .ok_or_else(|| {
                tonic::Status::failed_precondition(
                    "No upload task found for the specified build ID. Build may still be in progress or upload not scheduled.",
                )
            })?
            .clone();
        drop(upload_tasks);
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
                    let mut buffer = vec![0u8; 1024 * 64]; // 64KB buffer
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
