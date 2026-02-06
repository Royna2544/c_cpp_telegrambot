mod grpc_pb {
    include!(concat!(env!("OUT_DIR"), "/tgbot.builder.linuxkernel.rs"));
}
use super::builder_config::Toolchain;
use super::builder_config::{BuilderConfig, CompilerType};
use super::kernel_config::KernelConfig;
use crate::git_repo::GitRepo;
use chrono::Local;
use grpc_pb::ArtifactChunk;
use grpc_pb::ArtifactMetadata;
use grpc_pb::BuildPrepareRequest;
use grpc_pb::BuildRequest;
use grpc_pb::BuildStatus;
use grpc_pb::Config;
use grpc_pb::ConfigResponse;
use grpc_pb::ProgressStatus;
pub use grpc_pb::linux_kernel_build_service_server;
#[cfg(unix)]
use nix::sys::signal::{self, Signal};
#[cfg(unix)]
use nix::unistd::Pid;
use std::fs::File;
use std::io::Read;
use std::path::Path;
use std::path::PathBuf;
use std::pin::Pin;
use std::process::Stdio;
use std::sync::Arc;
use std::thread::available_parallelism;
use tokio::fs;
use tokio::io::AsyncWriteExt;
use tokio::io::{AsyncBufReadExt, BufReader};
use tokio::process::Command;
use tokio::sync::Mutex;
use tokio::sync::mpsc;
use tokio_stream::Stream;
use tokio_stream::wrappers::ReceiverStream;
use tonic::{Request, Response, Status};
use tracing::Instrument;
use tracing::{debug, error, info, warn};
use zip::CompressionMethod;
use zip::ZipWriter;
use zip::write::FileOptions; // for .process_group()

#[derive(Clone)]
pub struct BuildContext {
    id: i32,
    // Associated kernel config
    config: KernelConfig,
    // Associated toolchain entry
    toolchain: Toolchain,
    // Directory where the build is taking place
    work_dir: PathBuf,
    // Directory where toolchain is located
    toolchain_dir: PathBuf,
    // Target device name
    device_name: String,
    // Optional path to store the built artifact
    // If success, this stores the artifact (Whether it is .zip or raw kernel image)
    // If failure, this contains build logs
    // If cancelled, this is None
    artifact_path: Option<PathBuf>,
    // Optional kill signal sender
    kill_signal: Option<tokio::sync::mpsc::Sender<()>>,
}

struct PerBuildIdStatus {
    build_id: i32,
    finished: bool,
    succeeded: bool,
}

pub struct BuildService {
    kernel_configs: Arc<Mutex<Vec<KernelConfig>>>,
    contexts: Arc<Mutex<Vec<BuildContext>>>,
    id: Arc<Mutex<i32>>,
    build_statuses: Arc<Mutex<Vec<PerBuildIdStatus>>>,
    builder_config: BuilderConfig,
    temp_directory: PathBuf,
    output_directory: PathBuf,
}
type WrappedBuildStatus = Arc<Mutex<Vec<PerBuildIdStatus>>>;
type WrappedContexts = Arc<Mutex<Vec<BuildContext>>>;

impl BuildService {
    pub fn new(
        kernel_configs: Vec<KernelConfig>,
        builder_config: BuilderConfig,
        temp_directory: PathBuf,
        output_directory: PathBuf,
    ) -> Self {
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

        let v = BuildService {
            kernel_configs: Arc::new(Mutex::new(kernel_configs)),
            contexts: Arc::new(Mutex::new(Vec::new())),
            id: Arc::new(Mutex::new(0)),
            build_statuses: Arc::new(Mutex::new(Vec::new())),
            builder_config,
            temp_directory,
            output_directory,
        };
        v
    }

    async fn add_artifact_path_to_context(
        contexts: &WrappedContexts,
        build_id: i32,
        archive_file_path: &Path,
    ) {
        let mut ctxs = contexts.lock().await;
        if let Some(ctx) = ctxs.iter_mut().find(|c| c.id == build_id) {
            ctx.artifact_path = Some(archive_file_path.to_path_buf());
        }
    }

    async fn mark_build_finished(peridstat: &WrappedBuildStatus, build_id: i32, success: bool) {
        let mut statuses = peridstat.lock().await;
        if let Some(entry) = statuses.iter_mut().find(|s| s.build_id == build_id) {
            entry.finished = true;
            entry.succeeded = success;
        }
    }

    async fn is_build_finished(peridstat: &WrappedBuildStatus, build_id: i32) -> bool {
        let statuses = peridstat.lock().await;
        if let Some(entry) = statuses.iter().find(|s| s.build_id == build_id) {
            entry.finished
        } else {
            false
        }
    }

    async fn is_valid_build_id(peridstat: &WrappedBuildStatus, build_id: i32) -> bool {
        let statuses = peridstat.lock().await;
        statuses.iter().any(|s| s.build_id == build_id)
    }

    async fn inc_and_get_build_id(id_lock: &Arc<Mutex<i32>>) -> i32 {
        let mut id_guard = id_lock.lock().await;
        *id_guard += 1;
        *id_guard
    }

    async fn zip_dir_with_filename(
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

    async fn run_command_with_logs(
        mut command: Command, // The configured tokio::process::Command
        tx: mpsc::Sender<Result<BuildStatus, Status>>, // Where to send logs
        build_id: Option<i32>, // To tag the logs
        mut kill_rx: Option<mpsc::Receiver<()>>, // Optional Kill Switch
        log_path: Option<PathBuf>, // Optional log file path to also write logs to
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

        // 3. Spawn Process
        let mut child = command
            .spawn()
            .map_err(|e| Status::internal(e.to_string()))?;
        let stdout = child.stdout.take().expect("stdout missing");
        let stderr = child.stderr.take().expect("stderr missing");

        let mut args = command
            .as_std()
            .get_args()
            .map(|s| s.to_string_lossy().to_string())
            .collect::<Vec<_>>();
        let argv0 = command.as_std().get_program().to_string_lossy().to_string();
        args = args
            .into_iter()
            .chain(std::iter::once(argv0))
            .rev()
            .collect::<Vec<_>>();
        let wd = command
            .as_std()
            .get_current_dir()
            .unwrap_or_else(|| Path::new("<unknown>").into())
            .display()
            .to_string();

        info!("Spawned command: args: [{}], wd: {}", args.join(" "), wd);
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
                    let _ = f.write_all(line.as_bytes()).await;
                    let _ = f.write_all(b"\n").await;
                }

                // B. Send to gRPC
                let _ = tx_out
                    .send(Ok(BuildStatus {
                        status: ProgressStatus::InProgressBuild.into(),
                        output: format!("stdout: {}", line),
                        build_id,
                    }))
                    .await;
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
                let _ = tx_err
                    .send(Ok(BuildStatus {
                        status: ProgressStatus::InProgressBuild.into(),
                        output: format!("stderr: {}", line),
                        build_id,
                    }))
                    .await;
            }
        });

        // 5. Wait for Exit or Kill (Select Logic)
        let success = if let Some(rx) = &mut kill_rx {
            tokio::select! {
                res = child.wait() => {
                    match res {
                        Ok(status) => {
                            info!("Process exited with status: {:?}", status);
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
                        if let Some(pid) = child.id() {
                            // Cast u32 -> i32 for the negative PID trick
                            let _ = signal::kill(Pid::from_raw(-(pid as i32)), Signal::SIGINT);
                        }
                    }
                    #[cfg(not(unix))]
                    {
                            let _ = child.kill().await;
                    }

                    // Abort log tasks
                    out_handle.abort();
                    err_handle.abort();

                    // Notify cancellation
                    let _ = tx.send(Ok(BuildStatus {
                        status: ProgressStatus::Failed.into(),
                        output: "Build cancelled.".into(),
                        build_id,
                    })).await;
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
                    info!("Process exited with status: {:?}", status);
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

macro_rules! report {
    ($tx:expr, $status:ident, $msg:expr) => {
        info!("Build Status - {:?}: {}", ProgressStatus::$status, $msg);
        $tx.send(Ok(BuildStatus {
            status: ProgressStatus::$status.into(),
            output: $msg.into(),
            build_id: None,
        }))
        .await
        .unwrap();
    };
}

macro_rules! report_blk {
    ($tx:expr, $status:ident, $msg:expr) => {
        info!("Build Status - {:?}: {}", ProgressStatus::$status, $msg);
        $tx.blocking_send(Ok(BuildStatus {
            status: ProgressStatus::$status.into(),
            output: $msg.into(),
            build_id: None,
        }))
        .unwrap();
    };
}

macro_rules! report_build_id {
    ($tx:expr, $build_id:expr, $status:ident, $msg:expr) => {
        info!("Build Status - {:?}: {}", ProgressStatus::$status, $msg);
        $tx.send(Ok(BuildStatus {
            status: ProgressStatus::$status.into(),
            output: $msg.into(),
            build_id: Some($build_id),
        }))
        .await
        .unwrap();
    };
}

#[tonic::async_trait]
impl linux_kernel_build_service_server::LinuxKernelBuildService for BuildService {
    type listConfigsStream =
        Pin<Box<dyn Stream<Item = Result<Config, tonic::Status>> + Send + 'static>>;
    type prepareBuildStream =
        Pin<Box<dyn Stream<Item = Result<BuildStatus, tonic::Status>> + Send + 'static>>;
    type doBuildStream =
        Pin<Box<dyn Stream<Item = Result<BuildStatus, tonic::Status>> + Send + 'static>>;
    type getArtifactStream =
        Pin<Box<dyn Stream<Item = Result<ArtifactChunk, tonic::Status>> + Send + 'static>>;

    async fn add_config(
        &self,
        request: Request<Config>,
    ) -> Result<Response<ConfigResponse>, Status> {
        log_who_asked_me("add_config", &request);

        let req = request.into_inner();

        // 1. Validate JSON (The only "work" Rust does)
        let json_val: KernelConfig = match serde_json::from_str(&req.json_content) {
            Ok(v) => v,
            Err(e) => return Err(Status::invalid_argument(format!("Bad JSON: {}", e))),
        };

        // 2. Store the valid config in memory
        let mut configs = self.kernel_configs.lock().await;
        configs.push(json_val);

        Ok(Response::new(ConfigResponse {
            success: true,
            message: "Config saved successfully".into(),
        }))
    }

    async fn update_config(
        &self,
        request: Request<Config>,
    ) -> Result<Response<ConfigResponse>, Status> {
        log_who_asked_me("update_config", &request);
        let req = request.into_inner();

        // 1. Validate JSON
        let json_val: KernelConfig = match serde_json::from_str(&req.json_content) {
            Ok(v) => v,
            Err(e) => return Err(Status::invalid_argument(format!("Bad JSON: {}", e))),
        };

        // 2. Update existing config in memory
        let mut configs = self.kernel_configs.lock().await;
        if let Some(existing_config) = configs.iter_mut().find(|c| c.name == json_val.name) {
            *existing_config = json_val;
            Ok(Response::new(ConfigResponse {
                success: true,
                message: "Config updated successfully".into(),
            }))
        } else {
            Err(Status::not_found("Config not found"))
        }
    }

    async fn list_configs(
        &self,
        request: tonic::Request<()>,
    ) -> std::result::Result<tonic::Response<Self::listConfigsStream>, tonic::Status> {
        log_who_asked_me("list_configs", &request);

        let configs = self.kernel_configs.lock().await;
        debug!("Number of configs: {}", configs.len());
        let response_items: Vec<Result<Config, tonic::Status>> = configs
            .iter()
            .map(|config| {
                Ok(Config {
                    name: config.name.clone(),
                    json_content: serde_json::to_string(config).unwrap_or_default(),
                })
            })
            .collect();

        // Create a stream from the vector of responses
        let output = tokio_stream::iter(response_items);
        Ok(Response::new(Box::pin(output)))
    }

    async fn delete_config(
        &self,
        request: Request<Config>,
    ) -> Result<Response<ConfigResponse>, Status> {
        log_who_asked_me("delete_config", &request);
        let req = request.into_inner();

        // 1. Remove config from memory
        let mut configs = self.kernel_configs.lock().await;
        let initial_len = configs.len();
        configs.retain(|c| c.name != req.name);

        if configs.len() < initial_len {
            Ok(Response::new(ConfigResponse {
                success: true,
                message: "Config deleted successfully".into(),
            }))
        } else {
            Err(Status::not_found("Config not found"))
        }
    }

    async fn prepare_build(
        &self,
        request: Request<BuildPrepareRequest>,
    ) -> std::result::Result<tonic::Response<Self::prepareBuildStream>, tonic::Status> {
        log_who_asked_me("prepare_build", &request);

        // Create a channel for streaming responses
        let (tx, rx) = mpsc::channel(100);
        let config_handle = self.kernel_configs.clone();
        let builder_config = self.builder_config.clone();
        let outdir = self.output_directory.clone();
        let tmp_dir = self.temp_directory.clone();
        let contexts_handle = self.contexts.clone();
        let build_id_handle = self.id.clone();
        let per_build_statuses = self.build_statuses.clone();
        let tx_for_final = tx.clone();

        let spawnres: tokio::task::JoinHandle<Result<(), Status>> = tokio::spawn(async move {
            report!(
                tx,
                Pending,
                "Starting build preparation, awaiting to acquire lock..."
            );

            let req = request.into_inner();

            let configs = config_handle.lock().await;

            report!(
                tx,
                InProgressConfigure,
                "Lock acquired, searching for config..."
            );

            // Find the requested config
            let config = match configs.iter().find(|c| c.name == req.name) {
                Some(c) => {
                    let new = c.clone();
                    drop(configs);
                    new
                }
                None => {
                    report!(
                        tx,
                        Failed,
                        format!("Config not found by name: {}", req.name)
                    );
                    return Ok(());
                }
            };

            report!(
                tx,
                InProgressConfigure,
                "Config found, validating device..."
            );

            // Find requested device
            let device_entry = config
                .defconfig
                .devices
                .iter()
                .find(|d| *d == &req.device_name);
            if device_entry.is_none() {
                report!(tx, Failed, format!("Device {} not found", req.device_name));
                return Ok(());
            }

            report!(
                tx,
                InProgressConfigure,
                "Device validated, checking fragments..."
            );

            // Verify config fragments are known to us.
            for fragment in &req.config_fragments {
                if !config.fragments.iter().any(|f| f.name == *fragment) {
                    report!(tx, Failed, format!("Unknown fragment: {}", *fragment));
                    return Ok(());
                }
            }

            report!(
                tx,
                InProgressConfigure,
                "Fragments validated, checking toolchain support..."
            );

            // Parse clang/llvm support from the config
            let toolchain = if !config.toolchains.clang {
                // This suggests an old kernel. Look for GCC 4.9 or older
                let found = builder_config.toolchains.iter().find(|t| {
                    t.compiler == CompilerType::GCC
                        && t.compiler_version <= 4.9
                        && t.arch == config.arch
                });
                if let Some(tc) = found {
                    tc
                } else {
                    report!(
                        tx,
                        Failed,
                        format!(
                            "No suitable GCC toolchain found for architecture {:?}",
                            config.arch
                        )
                    );
                    return Ok(());
                }
            } else {
                // Look for any Clang toolchain
                let found = builder_config
                    .toolchains
                    .iter()
                    .find(|t| t.compiler == CompilerType::Clang && t.arch == config.arch);
                if let Some(tc) = found {
                    tc
                } else {
                    report!(
                        tx,
                        Failed,
                        format!(
                            "No suitable Clang toolchain found for architecture {:?}",
                            config.arch
                        )
                    );
                    return Ok(());
                }
            };

            // Try to use exisiting toolchain dir or clone if missing
            let toolchain_dir = outdir.join(toolchain.name.clone());
            let toolchain_found = match toolchain.exec_and_get_version(&toolchain_dir) {
                Some(ver) => {
                    report!(
                        tx,
                        InProgressConfigure,
                        format!("Toolchain {:?} found with version: {}", toolchain.name, ver)
                    );
                    true
                }
                None => {
                    report!(
                        tx,
                        InProgressConfigure,
                        format!("Toolchain {:?} not found or invalid.", toolchain.name)
                    );
                    false
                }
            };

            // Clone toolchain if not found
            if !toolchain_found {
                report!(
                    tx,
                    InProgressConfigure,
                    format!("Cloning toolchain {:?}...", toolchain.name)
                );
                match toolchain.clone_to_dir(&toolchain_dir).await {
                    Ok(_) => {}
                    Err(e) => {
                        report!(
                            tx,
                            Failed,
                            format!("Failed to clone toolchain {:?}: {}", toolchain.name, e)
                        );
                        return Ok(());
                    }
                }
                report!(
                    tx,
                    InProgressConfigure,
                    format!("Toolchain {:?} cloned successfully.", toolchain.name)
                );
            } else {
                report!(
                    tx,
                    InProgressConfigure,
                    format!(
                        "Toolchain directory {:?} exists, skipping download.",
                        toolchain_dir
                    )
                );
            }

            // Run version check
            let _version = toolchain
                .exec_and_get_version(&toolchain_dir)
                .ok_or_else(|| {
                    Status::internal(format!(
                        "Failed to execute toolchain {:?} after preparation.",
                        toolchain.name
                    ))
                })?;

            report!(
                tx,
                InProgressDownload,
                "Preparing to clone kernel source..."
            );

            // Now let us clone the kernel source...
            let tx_for_git = tx.clone();
            let source_dir = outdir.join(&config.name.replace(' ', "_").replace(':', "_"));
            let source_dir_clone = source_dir.clone();
            let source_dir_clone2 = source_dir.clone();
            let config_branch = config.repo.branch.clone();
            let config_url = config.repo.url.clone();
            let res: Result<Result<(), Status>, tokio::task::JoinError> =
                tokio::task::spawn_blocking(move || {
                    let tx_for_inner = tx_for_git.clone();
                    let tx_for_callback = tx_for_git.clone();
                    // Open repo...
                    let repo = GitRepo::new(
                        &source_dir_clone2,
                        "origin",
                        req.github_token.clone(),
                        Some(Box::new(move |stats| {
                            report_blk!(
                                tx_for_callback,
                                InProgressDownload,
                                format!(
                                    "[GIT OPERATION]: {}/{} objects received",
                                    stats.received_objects(),
                                    stats.total_objects()
                                )
                            );
                        })),
                    );
                    // Check url match and branch...
                    if let Ok(existing_repo) = repo {
                        match existing_repo.get_remote_url() {
                            Ok(url) => {
                                if url == config_url {
                                    report_blk!(
                                        tx_for_inner,
                                        InProgressDownload,
                                        "Existing repository URL matches config, skipping checkout."
                                    );
                                } else {
                                    // Try to checkout the correct branch
                                    if let Err(e) = existing_repo.checkout_branch(&config_branch) {
                                        report_blk!(
                                            tx_for_inner,
                                            InProgressDownload,
                                            format!(
                                                "Failed to checkout branch {}. Error: {}",
                                                config_branch, e
                                            )
                                        );
                                        return Ok(());
                                    } else {
                                        report_blk!(
                                            tx_for_inner,
                                            InProgressDownload,
                                            format!(
                                                "Checked out branch {} successfully.",
                                                config_branch
                                            )
                                        );
                                    }
                                }
                            }
                            Err(_e) => {
                                report_blk!(
                                    tx_for_inner,
                                    Failed,
                                    "Failed to get existing repository URL, aborting."
                                );
                                return Ok(());
                            }
                        }

                        // Try to fast-forward
                        match existing_repo.fast_forward() {
                            Ok(_) => {
                                report_blk!(
                                    tx_for_inner,
                                    InProgressDownload,
                                    "Repository fast-forwarded successfully."
                                );
                            }
                            Err(e) => {
                                report_blk!(
                                    tx_for_inner,
                                    Failed,
                                    format!("Failed to fast-forward repository. Error: {}", e)
                                );
                            }
                        }
                        Ok(())
                    } else {
                        match GitRepo::clone(
                            &config_url,
                            &config_branch,
                            None,
                            &source_dir_clone,
                            req.github_token.clone(),
                            &None,
                        ) {
                            Ok(_) => {
                                report_blk!(
                                    tx_for_inner,
                                    InProgressDownload,
                                    "Repository cloned successfully."
                                );
                            }
                            Err(e) => {
                                report_blk!(
                                    tx_for_inner,
                                    Failed,
                                    format!("Failed to clone repository. Error: {}", e)
                                );
                                return Ok(());
                            }
                        }

                        report_blk!(
                            tx_for_inner,
                            InProgressDownload,
                            "Kernel source cloned successfully."
                        );
                        Ok(())
                    }
                })
                .await;
            match res {
                Ok(inner_res) => {
                    if let Err(e) = inner_res {
                        report!(tx, Failed, format!("Git clone task failed: {}", e));
                        return Ok(());
                    }
                }
                Err(e) => {
                    report!(tx, Failed, format!("Git clone task failed: {}", e));
                    return Ok(());
                }
            }
            report!(
                tx,
                InProgressConfigure,
                "Kernel source cloned successfully."
            );
            report!(tx, InProgressConfigure, "Now will make defconfig...");

            let mut defconfig_name = config.defconfig.scheme.clone();
            defconfig_name = defconfig_name.replace("{device}", &req.device_name);
            let mut proc = Command::new("make");
            proc.current_dir(&source_dir)
                .arg(format!("-j{}", available_parallelism().unwrap().get()))
                .arg("O=out")
                .arg(&defconfig_name);

            for arg in toolchain.build_args(&config.arch) {
                proc.arg(arg);
            }
            for arg in &config.build_args() {
                proc.arg(arg);
            }

            for fragment in &req.config_fragments {
                if let Some(frag) = config.fragments.iter().find(|f| f.name == *fragment) {
                    let fragname = frag.scheme.clone().replace("{device}", &req.device_name);
                    info!("Adding fragment to make defconfig: {}", frag.name);
                    proc.arg(fragname);
                }
            }

            for (name, value) in config.env_vars(toolchain_dir.clone()) {
                info!("Setting environment variable for make: {}={}", name, value);
                proc.env(name.clone(), value.clone());
            }

            // Old kernel version requires manual creation of out folder
            let out_path = source_dir.join("out");
            if out_path.is_dir() {
                info!(
                    "Output directory {:?} already exists, skipping creation.",
                    out_path
                );
            } else {
                info!(
                    "Output directory {:?} does not exist, creating...",
                    out_path
                );
                std::fs::create_dir(out_path).map_err(|e| {
                    Status::internal(format!("Failed to create 'out' directory: {}", e))
                })?;
            }

            debug!("Running make defconfig in directory: {:?}", &source_dir);
            let log_file = tmp_dir.join(format!("output-prepare-{}.log", &config.name));
            info!("Defconfig log file will be at: {:?}", &log_file);
            let success =
                BuildService::run_command_with_logs(proc, tx.clone(), None, None, Some(log_file))
                    .await?;

            if !success {
                report!(tx, Failed, "make defconfig failed.");
                return Ok(());
            }

            report!(
                tx,
                InProgressConfigure,
                "make defconfig completed successfully."
            );

            let current_id = Self::inc_and_get_build_id(&build_id_handle).await;
            let entry = BuildContext {
                id: current_id,
                work_dir: source_dir,
                config: config,
                toolchain_dir: toolchain_dir,
                device_name: req.device_name,
                toolchain: toolchain.clone(),
                artifact_path: None,
                kill_signal: None,
            };
            let mut contexts = contexts_handle.lock().await;
            contexts.push(entry);
            drop(contexts);

            let mut per_build_statuses_lock = per_build_statuses.lock().await;
            per_build_statuses_lock.push(PerBuildIdStatus {
                build_id: current_id,
                finished: false,
                succeeded: success,
            });
            drop(per_build_statuses_lock);

            tx.send(Ok(BuildStatus {
                status: ProgressStatus::Success.into(),
                output: format!(
                    "Build preparation completed successfully., Build ID: {}",
                    current_id
                ),
                build_id: Some(current_id),
            }))
            .await
            .unwrap();
            Ok(())
        });

        tokio::spawn(async move {
            match spawnres.await {
                Ok(inner_res) => {
                    if let Err(e) = inner_res {
                        report!(
                            tx_for_final,
                            Failed,
                            format!("Build preparation task failed: {}", e)
                        );
                    } else {
                        info!("Build preparation task completed successfully.");
                    }
                }
                Err(e) => {
                    report!(
                        tx_for_final,
                        Failed,
                        format!("Build preparation task failed: {}", e)
                    );
                }
            }
        });

        let stream = ReceiverStream::new(rx);
        Ok(tonic::Response::new(
            Box::pin(stream) as Self::prepareBuildStream
        ))
    }

    async fn do_build(
        &self,
        request: Request<BuildRequest>,
    ) -> std::result::Result<tonic::Response<Self::doBuildStream>, tonic::Status> {
        log_who_asked_me("do_build", &request);

        let (tx, rx) = mpsc::channel(100);
        let req = request.into_inner();

        report_build_id!(
            tx,
            req.build_id,
            Pending,
            "Starting build process, validating build ID..."
        );

        // Validate build ID
        let peridstat = self.build_statuses.clone();
        if !Self::is_valid_build_id(&peridstat, req.build_id).await {
            report_build_id!(
                tx,
                req.build_id,
                Failed,
                format!("No build found with ID: {}", req.build_id)
            );
            return Err(Status::not_found(format!(
                "No build found with ID: {}",
                req.build_id
            )));
        }
        if Self::is_build_finished(&peridstat, req.build_id).await {
            report_build_id!(
                tx,
                req.build_id,
                Failed,
                format!(
                    "Build ID {} is not in a pending state. (Already finished?)",
                    req.build_id
                )
            );
            return Err(Status::failed_precondition(format!(
                "Build ID {} is not in a pending state. (Already finished?)",
                req.build_id
            )));
        }

        let context = {
            let contexts = self.contexts.lock().await;
            let ctx = contexts.iter().find(|c| c.id == req.build_id);
            match ctx {
                Some(c) => {
                    let cloned = c.clone();
                    drop(contexts);
                    cloned
                }
                None => {
                    report_build_id!(
                        tx,
                        req.build_id,
                        Failed,
                        format!("No build context found for ID: {}", req.build_id)
                    );
                    return Err(Status::not_found(format!(
                        "No build context found for ID: {}",
                        req.build_id
                    )));
                }
            }
        };
        let peridstat = self.build_statuses.clone();
        let toolchain_dir = context.toolchain_dir.clone();
        let tmp_dir = self.temp_directory.clone();
        let contexts_clone = self.contexts.clone();
        let toolchain = context.toolchain.clone();

        let (tx_kill, rx_kill) = mpsc::channel(1);

        // 2. Store the trigger (tx_kill) in the context so cancel_build can find it
        {
            let mut contexts = contexts_clone.lock().await;
            if let Some(ctx) = contexts.iter_mut().find(|c| c.id == req.build_id) {
                ctx.kill_signal = Some(tx_kill);
            }
        }

        let span = tracing::info_span!("build_watcher", build_id = req.build_id);
        let spawnres: tokio::task::JoinHandle<Result<(), Status>> = tokio::spawn(async move {
            report_build_id!(tx, req.build_id, Pending, "Build started...");

            let mut proc = Command::new("make");
            proc.current_dir(&context.work_dir)
                .arg(format!("-j{}", available_parallelism().unwrap().get()))
                .arg("O=out");

            for arg in toolchain.build_args(&context.config.arch) {
                proc.arg(arg);
            }
            for arg in &context.config.build_args() {
                proc.arg(arg);
            }

            for (name, value) in context.config.env_vars(toolchain_dir.clone()) {
                info!("Setting environment variable for make: {}={}", name, value);
                proc.env(name.clone(), value.clone());
            }
            let log_file = tmp_dir.join(format!("output-build-{}.log", &context.config.name));
            info!("Build log file will be at: {:?}", &log_file);
            debug!("Running make in directory: {:?}", &context.work_dir);

            let success = BuildService::run_command_with_logs(
                proc,
                tx.clone(),
                Some(req.build_id),
                Some(rx_kill),
                Some(log_file.clone()),
            )
            .await?;

            if !success {
                Self::add_artifact_path_to_context(&contexts_clone, req.build_id, &log_file).await;
                Self::mark_build_finished(&peridstat, req.build_id, false).await;
                info!("Build ID {} failed.", req.build_id);
                report_build_id!(tx, req.build_id, Failed, "Build failed.");
                return Ok(());
            }
            report_build_id!(tx, req.build_id, InProgressBuild, "Build succeeded.");
            let kernel_image = context
                .work_dir
                .join("out")
                .join("arch")
                .join(context.config.arch.to_string())
                .join("boot")
                .join(&context.config.image_type);
            let mut artifact = kernel_image.clone();

            // Check if kernel image exists
            if !artifact.exists() {
                Self::add_artifact_path_to_context(&contexts_clone, req.build_id, &log_file).await;
                Self::mark_build_finished(&peridstat, req.build_id, false).await;
                report_build_id!(
                    tx,
                    req.build_id,
                    Failed,
                    format!("Expected kernel image not found at {:?}", &kernel_image)
                );
                return Ok(());
            }

            report_build_id!(
                tx,
                req.build_id,
                InProgressBuild,
                format!("Kernel image located at {:?}", &kernel_image)
            );
            // Package with AnyKernel if configured
            if let Some(anykernel) = &context.config.anykernel
                && anykernel.enabled
            {
                report_build_id!(
                    tx,
                    req.build_id,
                    InProgressBuild,
                    "Packaging with AnyKernel..."
                );
                if anykernel.location.is_none() {
                    report_build_id!(
                        tx,
                        req.build_id,
                        InProgressBuild,
                        "AnyKernel packaging config is invalid, skipping."
                    );
                } else {
                    let anykernel_dir = context.work_dir.join(anykernel.location.as_ref().unwrap());
                    if !anykernel_dir.exists() {
                        Self::mark_build_finished(&peridstat, req.build_id, false).await;
                        report_build_id!(
                            tx,
                            req.build_id,
                            Failed,
                            format!("AnyKernel directory {:?} does not exist.", anykernel_dir)
                        );
                        return Ok(());
                    }

                    // Copy kernel image into AnyKernel directory
                    let dest_image_path =
                        anykernel_dir.join(PathBuf::from(&context.config.image_type));
                    match fs::copy(&kernel_image, &dest_image_path)
                        .await {
                        Ok(_) => {}
                        Err(e) => {
                            report_build_id!(
                                tx,
                                req.build_id,
                                Failed,
                                format!(
                                    "Failed to copy kernel image to AnyKernel directory: {}",
                                    e
                                )
                            );
                            return Ok(());
                        }
                    }

                    let date = Local::now();
                    let formatted_date = format!("{}", date.format("%Y-%m-%d_%H-%M-%S"));
                    let zip_file_path = context.work_dir.join(format!(
                        "{}_{}-{}.zip",
                        context.config.name, context.device_name, formatted_date
                    ));

                    report_build_id!(
                        tx,
                        req.build_id,
                        InProgressBuild,
                        format!("CreateZipFile {}", &zip_file_path.to_path_buf().display())
                    );
                    Self::zip_dir_with_filename(&zip_file_path, &anykernel_dir).await?;
                    report_build_id!(
                        tx,
                        req.build_id,
                        InProgressBuild,
                        "AnyKernel packaging complete."
                    );

                    // Delete the copied kernel image from AnyKernel directory
                    match fs::remove_file(&dest_image_path).await {
                        Ok(_) => {}
                        Err(e) => {
                            warn!(
                                "Failed to delete temporary kernel image from AnyKernel directory: {}",
                                e
                            );
                        }
                    };
                    artifact = zip_file_path;
                }
            } else {
                // Upload kernel image directly
            }

            report_build_id!(
                tx,
                req.build_id,
                Success,
                "Build complete. Dropping build context."
            );
            Self::add_artifact_path_to_context(&contexts_clone, req.build_id, &artifact).await;

            // Mark build as finished
            Self::mark_build_finished(&peridstat, req.build_id, true).await;
            Ok(())
        }.instrument(span));

        let span = tracing::info_span!("build_task", build_id = req.build_id);
        tokio::spawn(
            async move {
                match spawnres.await {
                    Ok(inner_res) => {
                        if let Err(e) = inner_res {
                            error!("Build task failed: {}", e);
                        } else {
                            info!("Build task completed successfully.");
                        }
                    }
                    Err(e) => {
                        error!("Build task failed to join: {}", e);
                    }
                }
            }
            .instrument(span),
        );

        let stream = ReceiverStream::new(rx);
        Ok(tonic::Response::new(Box::pin(stream) as Self::doBuildStream))
    }

    async fn cancel_build(
        &self,
        request: Request<BuildRequest>,
    ) -> Result<Response<BuildStatus>, Status> {
        log_who_asked_me("cancel_build", &request);
        let req = request.into_inner();

        // 0. Check if finishied already
        let per_build_statuses = self.build_statuses.clone();
        if !Self::is_valid_build_id(&per_build_statuses, req.build_id).await {
            return Err(Status::not_found(format!(
                "No build found with ID: {}",
                req.build_id
            )));
        }
        if Self::is_build_finished(&per_build_statuses, req.build_id).await {
            return Err(Status::failed_precondition(format!(
                "Build ID {} is not in a pending state. (Already finished?)",
                req.build_id
            )));
        }

        // 1. Get the Kill Signal Sender
        let contexts = self.contexts.lock().await;
        let kill_tx = contexts
            .iter()
            .find(|c| c.id == req.build_id)
            .and_then(|c| c.kill_signal.clone()); // Clone the sender

        drop(contexts); // Unlock immediately

        // 2. Trigger it
        match kill_tx {
            Some(tx) => {
                // Send the signal. We don't care if it fails (process might already be dead)
                let _ = tx.send(()).await;

                // Acquire lock to mark as finished
                let mut per_build_statuses_lock = self.build_statuses.lock().await;
                if let Some(entry) = per_build_statuses_lock
                    .iter_mut()
                    .find(|s| s.build_id == req.build_id)
                {
                    entry.finished = true;
                }
                drop(per_build_statuses_lock);

                Ok(Response::new(BuildStatus {
                    status: ProgressStatus::Failed.into(),
                    output: format!("Cancellation signal sent for Build ID {}", req.build_id),
                    build_id: Some(req.build_id),
                }))
            }
            None => Err(Status::not_found(format!(
                "Build ID {} not found or not running (cannot cancel)",
                req.build_id
            ))),
        }
    }

    async fn get_artifact(
        &self,
        request: Request<BuildRequest>,
    ) -> std::result::Result<
        tonic::Response<
            Pin<
                Box<
                    dyn Stream<Item = Result<grpc_pb::ArtifactChunk, tonic::Status>>
                        + Send
                        + 'static,
                >,
            >,
        >,
        tonic::Status,
    > {
        log_who_asked_me("get_artifact", &request);

        if !Self::is_valid_build_id(&self.build_statuses, request.get_ref().build_id).await {
            return Err(Status::not_found(format!(
                "No build found with ID: {}",
                request.get_ref().build_id
            )));
        }
        let peridstat = self.build_statuses.clone();
        if !Self::is_build_finished(&peridstat, request.get_ref().build_id).await {
            return Err(Status::failed_precondition(format!(
                "Build ID {} is not in a pending state. (Already finished?)",
                request.get_ref().build_id
            )));
        }
        let context = {
            let contexts = self.contexts.lock().await;
            let ctx = contexts
                .iter()
                .find(|c| c.id == request.get_ref().build_id)
                .unwrap();
            ctx.clone()
        };

        let req = request.into_inner();
        let (tx, rx) = mpsc::channel(100);
        let spawnres = tokio::spawn(async move {
            // Check if artifact exists
            let artifact_path = match &context.artifact_path {
                Some(p) => p.clone(),
                None => {
                    let _ = tx
                        .send(Err(Status::not_found(format!(
                            "No artifact found for build ID: {}",
                            req.build_id
                        ))))
                        .await;
                    return;
                }
            };

            info!(
                "Streaming artifact for build ID {} from path {:?}",
                req.build_id, artifact_path
            );
            // First send metadata chunk
            let artifact_meta = ArtifactMetadata {
                filename: artifact_path
                    .file_name()
                    .and_then(|s| s.to_str())
                    .unwrap_or("artifact.bin")
                    .to_string(),
                total_size: std::fs::metadata(&artifact_path)
                    .map_err(|e| {
                        Status::internal(format!("Failed to get metadata for artifact file: {}", e))
                    })
                    .unwrap()
                    .len(),
            };
            let meta_chunk = grpc_pb::ArtifactChunk {
                content: Some(grpc_pb::artifact_chunk::Content::Metadata(artifact_meta)),
            };
            if tx.send(Ok(meta_chunk)).await.is_err() {
                // Client disconnected
                error!("Client disconnected before receiving metadata chunk.");
                return;
            }
            info!("Sent artifact metadata for build ID {}", req.build_id);

            // Stream the artifact file in chunks
            let mut file = match File::open(&artifact_path) {
                Ok(f) => f,
                Err(e) => {
                    let _ = tx
                        .send(Err(Status::internal(format!(
                            "Failed to open artifact file: {}",
                            e
                        ))))
                        .await;
                    return;
                }
            };
            let mut buffer = [0u8; 8192];
            loop {
                match file.read(&mut buffer) {
                    Ok(0) => break, // EOF
                    Ok(n) => {
                        let chunk = grpc_pb::ArtifactChunk {
                            content: Some(grpc_pb::artifact_chunk::Content::Data(
                                buffer[..n].to_vec(),
                            )),
                        };
                        if tx.send(Ok(chunk)).await.is_err() {
                            // Client disconnected
                            error!("Client disconnected while streaming artifact data.");
                            break;
                        }
                    }
                    Err(e) => {
                        let _ = tx
                            .send(Err(Status::internal(format!(
                                "Failed to read artifact file: {}",
                                e
                            ))))
                            .await;
                        return;
                    }
                }
            }
        });

        tokio::spawn(async move {
            match spawnres.await {
                Ok(_) => {
                    info!("Completed streaming artifact for build ID {}", req.build_id);
                }
                Err(e) => {
                    error!(
                        "Artifact streaming task failed to join for build ID {}: {}",
                        req.build_id, e
                    );
                }
            }
        });

        let stream = ReceiverStream::new(rx);
        Ok(tonic::Response::new(
            Box::pin(stream) as Self::getArtifactStream
        ))
    }
}
