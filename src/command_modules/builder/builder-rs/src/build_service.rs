mod grpc_pb {
    include!(concat!(env!("OUT_DIR"), "/tgbot.builder.linuxkernel.rs"));
}
pub const FILE_DESCRIPTOR_SET: &[u8] = tonic::include_file_descriptor_set!("descriptor");
use crate::build_service::grpc_pb::ArtifactChunk;
use crate::build_service::grpc_pb::ArtifactMetadata;
use crate::build_service::grpc_pb::BuildPrepareRequest;
use crate::build_service::grpc_pb::BuildRequest;
use crate::build_service::grpc_pb::BuildStatus;
use crate::build_service::grpc_pb::Config;
use crate::build_service::grpc_pb::ConfigResponse;
use crate::build_service::grpc_pb::ProgressStatus;
pub use crate::build_service::grpc_pb::linux_kernel_build_service_server;
use crate::builder_config;
use crate::builder_config::Architecture;
use crate::builder_config::{BuilderConfig, CompilerType};
use crate::kernel_config;
use crate::kernel_config::EnvVar;
use crate::kernel_config::KernelConfig;
use chrono::Local;
use flate2::read::GzDecoder; // For .tar.gz
use futures_util::StreamExt;
use git2::FetchOptions;
use git2::Repository;
use git2::build;
use git2::build::RepoBuilder;
use std::fmt::Debug;
use std::fs::File;
use std::io::Read;
use std::path::Path;
use std::path::PathBuf;
use std::pin::Pin;
use std::process::ExitStatus;
use std::process::Stdio;
use std::sync::Arc;
use std::thread::available_parallelism;
use tar::Archive;
use tokio::fs::File as TokioFile;
use tokio::io::AsyncWriteExt;
use tokio::io::{AsyncBufReadExt, BufReader};
use tokio::process::Command;
use tokio::sync::Mutex;
use tokio::sync::mpsc;
use tokio::time::Duration;
use tokio::time::Instant;
use tokio_stream::Stream;
use tokio_stream::wrappers::ReceiverStream;
use tonic::{Request, Response, Status};
use tracing::{debug, error, info, instrument, warn};
use tracing_subscriber::field::debug;
use zip::CompressionMethod;
use zip::ZipArchive;
use zip::ZipWriter;
use zip::write::FileOptions;

#[derive(Clone)]
pub struct BuildContext {
    id: i32,
    config: KernelConfig,
    work_dir: PathBuf,
    toolchain_dir: PathBuf,
    device_name: String,
    artifact_path: Option<PathBuf>,
    pub kill_signal: Option<tokio::sync::mpsc::Sender<()>>,
}

struct PerBuildIdStatus {
    build_id: i32,
    finished: bool,
}

pub struct BuildService {
    pub kernel_configs: Arc<Mutex<Vec<KernelConfig>>>,
    contexts: Arc<Mutex<Vec<BuildContext>>>,
    id: Arc<Mutex<i32>>,
    build_statuses: Arc<Mutex<Vec<PerBuildIdStatus>>>,
    pub builder_config: BuilderConfig,
    pub temp_directory: PathBuf,
    pub output_directory: PathBuf,
}

impl BuildService {
    pub fn new(
        kernel_configs: Vec<KernelConfig>,
        builder_config: BuilderConfig,
        temp_directory: PathBuf,
        output_directory: PathBuf,
    ) -> Self {
        BuildService {
            kernel_configs: Arc::new(Mutex::new(kernel_configs)),
            contexts: Arc::new(Mutex::new(Vec::new())),
            id: Arc::new(Mutex::new(0)),
            build_statuses: Arc::new(Mutex::new(Vec::new())),
            builder_config,
            temp_directory,
            output_directory,
        }
    }

    pub fn inc_build_id(&self) -> i32 {
        let mut id_lock = self.id.blocking_lock();
        *id_lock += 1;
        self.build_statuses.blocking_lock().push(PerBuildIdStatus {
            build_id: *id_lock,
            finished: false,
        });
        *id_lock
    }

    pub fn mark_build_finished(&self, build_id: i32) {
        let mut statuses = self.build_statuses.blocking_lock();
        if let Some(entry) = statuses.iter_mut().find(|s| s.build_id == build_id) {
            entry.finished = true;
        }
    }
}

async fn download_file<F, Fut>(
    url: &str,
    dest: &Path,
    progress: F,
) -> Result<(), Box<dyn std::error::Error>>
where
    F: Fn(u64, u64) -> Fut,
    Fut: Future<Output = ()>,
{
    let response = reqwest::get(url).await?;

    let mut downloaded_size = 0;

    // 1. Create the file
    let mut file = TokioFile::create(dest).await?;

    // 2. Stream the content
    let mut stream = response.bytes_stream();

    let mut last_update = Instant::now();
    let update_interval = Duration::from_secs(5);

    while let Some(item) = stream.next().await {
        let chunk = item?;
        if last_update.elapsed() >= update_interval || downloaded_size == 0 {
            progress(chunk.len() as u64, downloaded_size).await;
            last_update = Instant::now();
        }
        downloaded_size += chunk.len() as u64;
        file.write_all(&chunk).await?;
    }

    Ok(())
}

pub fn extract_tar_gz(archive_path: &Path, dest: &Path) -> Result<(), std::io::Error> {
    info!("Extracting {:?} to {:?}...", archive_path, dest);

    let tar_gz = File::open(archive_path)?;
    let tar = GzDecoder::new(tar_gz);
    let mut archive = Archive::new(tar);

    // This is equivalent to 'tar -xf archive.tar.gz -C dest'
    archive.unpack(dest)?;

    Ok(())
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
        debug!("add_config called");
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
        debug!("update_config called");
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
        debug!("list_configs called");

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
        debug!("delete_config called");
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
        debug!("prepare_build called");

        // Create a channel for streaming responses
        let (tx, rx) = mpsc::channel(100);
        let config_handle = self.kernel_configs.clone();
        let builder_config = self.builder_config.clone();
        let outdir = self.output_directory.clone();
        let contexts_handle = self.contexts.clone();
        let build_id_handle = self.id.clone();
        let per_build_statuses = self.build_statuses.clone();

        macro_rules! report {
            ($status:ident, $msg:expr) => {
                info!("Build Status - {:?}: {}", ProgressStatus::$status, $msg);
                // We capture 'tx' from the surrounding scope automatically
                tx.send(Ok(BuildStatus {
                    status: ProgressStatus::$status.into(),
                    output: $msg.into(),
                    build_id: None,
                }))
                .await
                .unwrap();
            };
        }

        let _ = tokio::spawn(async move {
            report!(
                Pending,
                "Starting build preparation, awaiting to acquire lock..."
            );

            let req = request.into_inner();

            let configs = config_handle.lock().await;

            report!(
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
                    report!(Failed, format!("Config not found by name: {}", req.name));
                    return Err(Status::not_found(format!(
                        "Config not found by name: {}",
                        req.name
                    )));
                }
            };

            report!(InProgressConfigure, "Config found, validating device...");

            // Find requested device
            let device_entry = config
                .defconfig
                .devices
                .iter()
                .find(|d| *d == &req.device_name);
            if device_entry.is_none() {
                report!(Failed, format!("Device {} not found", req.device_name));
                return Err(Status::not_found(format!(
                    "Device {} not found in config",
                    req.device_name
                )));
            }

            report!(
                InProgressConfigure,
                "Device validated, checking fragments..."
            );

            // Verify config fragments are known to us.
            for fragment in &req.config_fragments {
                if !config.fragments.iter().any(|f| f.name == *fragment) {
                    report!(Failed, format!("Unknown fragment: {}", *fragment));
                    return Err(Status::failed_precondition(format!(
                        "Unknown fragment: {}",
                        *fragment
                    )));
                }
            }

            report!(
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
                        Failed,
                        format!(
                            "No suitable GCC toolchain found for architecture {:?}",
                            config.arch
                        )
                    );
                    return Err(Status::failed_precondition(format!(
                        "No suitable GCC toolchain found for architecture {:?}",
                        config.arch
                    )));
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
                        Failed,
                        format!(
                            "No suitable Clang toolchain found for architecture {:?}",
                            config.arch
                        )
                    );
                    return Err(Status::failed_precondition(format!(
                        "No suitable Clang toolchain found for architecture {:?}",
                        config.arch
                    )));
                }
            };

            let toolchain_dir = outdir.join(toolchain.name.clone());
            let toolchain_found = match toolchain.exec_and_get_version(&toolchain_dir) {
                Some(ver) => {
                    report!(
                        InProgressConfigure,
                        format!("Toolchain {:?} found with version: {}", toolchain.name, ver)
                    );
                    true
                }
                None => {
                    report!(
                        InProgressConfigure,
                        format!("Toolchain {:?} not found or invalid.", toolchain.name)
                    );
                    false
                }
            };
            if !toolchain_found {
                if std::path::Path::new(&toolchain_dir).exists() {
                    report!(
                        InProgressConfigure,
                        format!(
                            "Output directory {:?} exists, but toolchain not found inside. Removing...",
                            toolchain_dir
                        )
                    );
                    std::fs::remove_dir_all(&toolchain_dir).map_err(|e| {
                        Status::internal(format!(
                            "Failed to remove incomplete toolchain directory {:?}: {}",
                            toolchain_dir, e
                        ))
                    })?;
                } else {
                    report!(
                        InProgressConfigure,
                        format!(
                            "Output directory {:?} does not exist, creating...",
                            toolchain_dir
                        )
                    );

                    if let Err(e) = std::fs::create_dir_all(&toolchain_dir) {
                        report!(
                            Failed,
                            format!(
                                "Failed to create output directory {:?}: {}",
                                toolchain_dir, e
                            )
                        );
                        return Err(Status::internal(format!(
                            "Failed to create output directory {:?}: {}",
                            toolchain_dir, e
                        )));
                    }
                }
                match toolchain.source_type {
                    builder_config::Source::Git => {
                        report!(
                            InProgressDownload,
                            format!(
                                "Will clone: {:?}-{:?} toolchain from Git.",
                                toolchain.url, toolchain.branch
                            )
                        );
                        Repository::clone(&toolchain.url, toolchain_dir.clone())
                            .map_err(|e| Status::internal(format!("Git clone failed: {}", e)))?;
                    }
                    builder_config::Source::Tarball => {
                        report!(
                            InProgressDownload,
                            format!(
                                "Will use: {:?} toolchain from tarball binaries.",
                                toolchain.url
                            )
                        );
                        let dest_path = toolchain_dir.join(format!("{}.tar.gz", toolchain.name));
                        download_file(&toolchain.url, &dest_path, async |current, total| {
                            report!(
                                InProgressDownload,
                                format!(
                                    "Downloading toolchain... Total downloaded {} KB",
                                    total / 1024
                                )
                            );
                        })
                        .await
                        .map_err(|e| Status::internal(format!("Download failed: {}", e)))?;

                        report!(
                            InProgressDownload,
                            format!("Download complete, extracting toolchain...")
                        );

                        extract_tar_gz(&dest_path, &toolchain_dir)
                            .map_err(|e| Status::internal(format!("Extraction failed: {}", e)))?;

                        report!(InProgressDownload, format!("Extraction complete."));
                    }
                }
            } else {
                report!(
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

            report!(InProgressDownload, "Preparing to clone kernel source...");

            // Now let us clone the kernel source...
            let tx_for_git = tx.clone();
            let source_dir = outdir.join(&config.name);
            let source_dir_clone = source_dir.clone();
            let config_branch = config.repo.branch.clone();
            let config_url = config.repo.url.clone();
            let res = tokio::task::spawn_blocking(move || {
                let tx_for_inner = tx_for_git.clone();
                let mut callbacks = git2::RemoteCallbacks::new();

                let repo = git2::Repository::open(&source_dir_clone).ok(); // Check if already cloned
                // Repo opens successfully, check if URL matches
                if let Some(r) = repo {
                    // Lookup origin remote
                    if let Ok(remote) = r.find_remote("origin") {
                        // Check URL
                        if let Some(url) = remote.url() {
                            // If URL does not match, reclone
                            if url != config_url {
                                tx_for_inner
                                    .blocking_send(Ok(BuildStatus {
                                        status: ProgressStatus::InProgressDownload.into(),
                                        output: "Existing repository URL does not match config, recloning.".into(),
                                        build_id: None,
                                    }))
                                    .unwrap();
                                std::fs::remove_dir_all(&source_dir_clone).map_err(|e| {
                                    Status::internal(format!(
                                        "Failed to remove mismatched repo directory: {}",
                                        e
                                    ))
                                })?;
                            }
                        }
                    }
                    // Check if branch matches
                        if let Ok(head_ref) = r.head() {
                            if let Some(name) = head_ref.shorthand() {
                                if name != config_branch {
                                    tx_for_inner
                                        .blocking_send(Ok(BuildStatus {
                                            status: ProgressStatus::InProgressDownload.into(),
                                            output: "Existing repository branch does not match config, trying to check out the correct branch.".into(),
                                            build_id: None,
                                        }))
                                        .unwrap();
                                    r.set_head(&format!("refs/heads/{}", config_branch)).map_err(|e| {
                                        Status::internal(format!(
                                            "Failed to set head to branch {}: {}",
                                            config_branch, e
                                        ))
                                    })?;
                                    r.checkout_head(None).map_err(|e| {
                                        Status::internal(format!(
                                            "Failed to checkout branch {}: {}",
                                            config_branch, e
                                        ))
                                    })?;
                                }
                            }

                    }

                    // Do a fast-forward pull to update the repo
                    let mut fo = git2::FetchOptions::new();
                    fo.remote_callbacks(callbacks);
                    let mut remote = r.find_remote("origin").map_err(|e| {
                        Status::internal(format!("Failed to find remote 'origin': {}", e))
                    })?;
                    remote.fetch(&[&config_branch], Some(&mut fo), None).map_err(|e| {
                        Status::internal(format!("Failed to fetch updates: {}", e))
                    })?;
                    let fetch_head = r.find_reference("FETCH_HEAD").map_err(|e| {
                        Status::internal(format!("Failed to find FETCH_HEAD: {}", e))
                    })?;
                    let fetch_commit = r.reference_to_annotated_commit(&fetch_head).map_err(|e| {
                        Status::internal(format!("Failed to get annotated commit: {}", e))
                    })?;
                    let analysis = r.merge_analysis(&[&fetch_commit]).map_err(|e| {
                        Status::internal(format!("Merge analysis failed: {}", e))
                    })?;
                    if analysis.0.is_fast_forward() {
                        let refname = format!("refs/heads/{}", config_branch);
                        let mut rhead = r.find_reference(&refname).map_err(|e| {
                            Status::internal(format!("Failed to find reference {}: {}", refname, e))
                        })?;
                        rhead.set_target(fetch_commit.id(), "Fast-Forward").map_err(|e| {
                            Status::internal(format!(
                                "Failed to set reference target: {}",
                                e
                            ))
                        })?;
                        r.checkout_head(None).map_err(|e| {
                            Status::internal(format!("Failed to checkout HEAD: {}", e))
                        })?;
                    }
                    tx_for_inner
                        .blocking_send(Ok(BuildStatus {
                            status: ProgressStatus::InProgressDownload.into(),
                            output: "Kernel source already cloned, skipping.".into(),
                            build_id: None,
                        }))
                        .unwrap();
                    return Ok(());
                }

                let mut last_update = Instant::now();
                let update_interval = Duration::from_secs(5);

                // Set up the progress callback
                callbacks.transfer_progress(move |stats| {
                    if last_update.elapsed() < update_interval {
                        return true;
                    }
                    last_update = Instant::now();
                    tx_for_inner
                        .blocking_send(Ok(BuildStatus {
                            status: ProgressStatus::InProgressDownload.into(),
                            output: format!(
                                "Cloning kernel source: {}/{} objects received",
                                stats.received_objects(),
                                stats.total_objects()
                            ),
                            build_id: None,
                        }))
                        .unwrap();

                    true // return true to continue download
                });

                callbacks.credentials(|url, username_from_url, _allowed_types| {
                    let config = git2::Config::open_default().unwrap();
                    let username = username_from_url.unwrap_or("git");
                    if url.starts_with("ssh://") {
                        return git2::Cred::ssh_key_from_agent(username);
                    }
                    return git2::Cred::credential_helper(&config, url, Some(username));
                });

                tx_for_git
                    .blocking_send(Ok(BuildStatus {
                        status: ProgressStatus::InProgressDownload.into(),
                        output: "Starting kernel source clone...".into(),
                        build_id: None,
                    }))
                    .unwrap();

                let mut fetch_options = FetchOptions::new();
                fetch_options.depth(req.clone_depth);
                fetch_options.remote_callbacks(callbacks);

                let mut builder = RepoBuilder::new();
                builder.fetch_options(fetch_options);
                builder.branch(&config_branch);

                let resu = builder.clone(&config_url, &source_dir_clone);
                let repo = match resu {
                    Ok(r) => r,
                    Err(e) => {
                        tx_for_git
                            .blocking_send(Ok(BuildStatus {
                                status: ProgressStatus::Failed.into(),
                                output: format!("Git clone failed: {}", e),
                                build_id: None,
                            }))
                            .unwrap();
                        return Err(Status::internal(format!("Git clone failed: {}", e)));
                    }
                };
                for mut submodule in repo
                    .submodules()
                    .map_err(|e| Status::internal(e.to_string()))?
                {
                    submodule
                        .update(true, None)
                        .map_err(|e| Status::internal(format!("Submodule error: {}", e)))?;
                }
                Ok(())
            })
            .await;
            match res {
                Ok(inner_res) => {
                    if let Err(e) = inner_res {
                        return Err(e);
                    }
                }
                Err(e) => {
                    report!(Failed, format!("Git clone task failed: {}", e));
                    return Err(Status::internal(format!("Git clone task failed: {}", e)));
                }
            }
            report!(InProgressConfigure, "Kernel source cloned successfully.");
            report!(InProgressConfigure, "Now will make defconfig...");

            let mut defconfig_name = config.defconfig.scheme.clone();
            defconfig_name = defconfig_name.replace("{device}", &req.device_name);
            let mut proc = Command::new("make");
            proc.current_dir(&source_dir)
                .arg(format!("-j{}", available_parallelism().unwrap().get()))
                .arg("O=out")
                .arg(&defconfig_name);

            match config.arch {
                Architecture::ARM => {
                    proc.arg("ARCH=arm");
                    proc.arg("CROSS_COMPILE=arm-linux-gnueabi-");
                }
                Architecture::ARM64 => {
                    proc.arg("ARCH=arm64");
                    proc.arg("CROSS_COMPILE=aarch64-linux-gnu-");
                }
                Architecture::X86 => {
                    proc.arg("ARCH=x86");
                    proc.arg("CROSS_COMPILE=x86_64-linux-gnu-");
                }
                Architecture::X86_64 => {
                    proc.arg("ARCH=x86");
                    proc.arg("CROSS_COMPILE=x86_64-linux-gnu-");
                }
                _ => {
                    error!("Unsupported architecture for make: {:?}", config.arch);
                }
            }

            if config.toolchains.clang {
                if config.toolchains.llvm_ias {
                    proc.arg("LLVM=1");
                    proc.arg("LLVM_IAS=1");
                } else if config.toolchains.llvm_binutils {
                    proc.arg("CC=clang");
                    proc.arg("LD=ld.lld");
                    proc.arg("AR=llvm-ar");
                    proc.arg("NM=llvm-nm");
                    proc.arg("OBJCOPY=llvm-objcopy");
                    proc.arg("OBJDUMP=llvm-objdump");
                    proc.arg("STRIP=llvm-strip");
                } else {
                    proc.arg("CC=clang");
                }
            }

            match config.arch {
                Architecture::ARM => {
                    proc.arg("ARCH=arm");
                }
                Architecture::ARM64 => {
                    proc.arg("ARCH=arm64");
                }
                Architecture::X86 => {
                    proc.arg("ARCH=x86");
                }
                Architecture::X86_64 => {
                    proc.arg("ARCH=x86");
                }
                _ => {
                    error!(
                        "Unsupported architecture for make defconfig: {:?}",
                        config.arch
                    );
                }
            }

            for fragment in &req.config_fragments {
                if let Some(frag) = config.fragments.iter().find(|f| f.name == *fragment) {
                    info!("Adding fragment to make defconfig: {}", frag.name);
                    proc.arg(frag.name.clone());
                }
            }

            for env_var in &config.env {
                info!(
                    "Setting environment variable for make defconfig: {}={}",
                    env_var.name, env_var.value
                );
                proc.env(env_var.name.clone(), env_var.value.clone());
            }

            // Ensure toolchain bin is in PATH
            let path = EnvVar {
                name: "PATH".into(),
                value: format!(
                    "{}:{}",
                    toolchain_dir.join("bin").to_string_lossy(),
                    std::env::var("PATH").unwrap_or_default()
                ),
            };
            proc.env(path.name, path.value);

            debug!("Running make defconfig in directory: {:?}", &source_dir);

            // 1. SETUP THE PROCESS
            proc.stdout(Stdio::piped());
            proc.stderr(Stdio::piped()); // Capture stderr too (make often prints info here)

            // 2. SPAWN THE PROCESS
            // We use spawn() instead of status() so we can interact with it while it runs.
            let mut child = proc
                .spawn()
                .map_err(|e| Status::internal(format!("Failed to spawn make: {}", e)))?;

            // 3. GRAB THE HANDLES
            // We take ownership of the pipes. If we don't, they close immediately.
            let stdout = child.stdout.take().expect("stdout not piped");
            let stderr = child.stderr.take().expect("stderr not piped");

            // 4. START LOG STREAMERS
            // We spawn two background tasks to read logs so they don't block each other.
            let tx_out = tx.clone();
            let tx_err = tx.clone();

            // Task A: Forward Stdout
            let out_handle = tokio::spawn(async move {
                let mut reader = BufReader::new(stdout).lines();
                while let Ok(Some(line)) = reader.next_line().await {
                    // We cannot use 'report!' here easily because of 'break',
                    // so we manually send.
                    let msg = BuildStatus {
                        status: ProgressStatus::InProgressBuild.into(),
                        output: format!("stdout: {}", line),
                        build_id: None,
                    };
                    // If client disconnects, stop reading
                    if tx_out.send(Ok(msg)).await.is_err() {
                        break;
                    }
                }
            });

            // Task B: Forward Stderr (Merged into the same stream)
            let err_handle = tokio::spawn(async move {
                let mut reader = BufReader::new(stderr).lines();
                while let Ok(Some(line)) = reader.next_line().await {
                    let msg = BuildStatus {
                        status: ProgressStatus::InProgressBuild.into(),
                        output: format!("stderr: {}", line),
                        build_id: None,
                    };
                    if tx_err.send(Ok(msg)).await.is_err() {
                        break;
                    }
                }
            });

            // 5. WAIT FOR COMPLETION
            // Wait for the process to exit...
            let status = child
                .wait()
                .await
                .map_err(|e| Status::internal(e.to_string()))?;

            // ...AND wait for the logs to finish flushing.
            // If we don't await these, we might lose the last few lines of logs.
            let _ = out_handle.await;
            let _ = err_handle.await;

            // 6. CHECK FINAL STATUS
            if !status.success() {
                report!(
                    Failed,
                    format!("make defconfig failed with status: {:?}", status)
                );
                return Err(Status::internal("make defconfig failed"));
            }
            report!(
                InProgressConfigure,
                "make defconfig completed successfully."
            );
            report!(Success, "Configuration complete.");

            let mut build_id_lock = build_id_handle.lock().await;
            *build_id_lock += 1;
            let current_id = *build_id_lock;
            drop(build_id_lock);

            let entry = BuildContext {
                id: current_id,
                work_dir: source_dir,
                config,
                toolchain_dir,
                device_name: req.device_name,
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
        let stream = ReceiverStream::new(rx);
        Ok(tonic::Response::new(
            Box::pin(stream) as Self::prepareBuildStream
        ))
    }

    async fn do_build(
        &self,
        request: Request<BuildRequest>,
    ) -> std::result::Result<tonic::Response<Self::doBuildStream>, tonic::Status> {
        debug!("do_build called");

        let (tx, rx) = mpsc::channel(100);
        let req = request.into_inner();
        macro_rules! report {
            ($status:ident, $msg:expr) => {
                // We capture 'tx' from the surrounding scope automatically
                tx.send(Ok(BuildStatus {
                    status: ProgressStatus::$status.into(),
                    output: $msg.into(),
                    build_id: Some(req.build_id),
                }))
                .await
                .unwrap();
            };
        }

        // Validate build ID
        let peridstat = self.build_statuses.lock().await;
        let build_status_entry = peridstat.iter().find(|s| s.build_id == req.build_id);
        if build_status_entry.is_none() {
            return Err(Status::not_found(format!(
                "No build found with ID: {}",
                req.build_id
            )));
        }
        let build_status_entry = build_status_entry.unwrap();
        if build_status_entry.finished {
            return Err(Status::failed_precondition(format!(
                "Build with ID {} is already finished.",
                req.build_id
            )));
        }
        drop(peridstat);

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
                    return Err(Status::not_found(format!(
                        "No build context found for ID: {}",
                        req.build_id
                    )));
                }
            }
        };
        let peridstat = self.build_statuses.clone();
        let toolchain_dir = context.toolchain_dir.clone();
        let contexts_clone = self.contexts.clone();

        let (tx_kill, mut rx_kill) = mpsc::channel(1);

        // 2. Store the trigger (tx_kill) in the context so cancel_build can find it
        {
            let mut contexts = contexts_clone.lock().await;
            if let Some(ctx) = contexts.iter_mut().find(|c| c.id == req.build_id) {
                ctx.kill_signal = Some(tx_kill);
            }
        }

        tokio::spawn(async move {
            report!(Pending, "Build started...");

            let mut proc = Command::new("make");
            proc.current_dir(&context.work_dir)
                .arg(format!("-j{}", available_parallelism().unwrap().get()))
                .arg("O=out");

            match context.config.arch {
                Architecture::ARM => {
                    proc.arg("ARCH=arm");
                    proc.arg("CROSS_COMPILE=arm-linux-gnueabi-");
                }
                Architecture::ARM64 => {
                    proc.arg("ARCH=arm64");
                    proc.arg("CROSS_COMPILE=aarch64-linux-gnu-");
                }
                Architecture::X86 => {
                    proc.arg("ARCH=x86");
                    proc.arg("CROSS_COMPILE=x86_64-linux-gnu-");
                }
                Architecture::X86_64 => {
                    proc.arg("ARCH=x86");
                    proc.arg("CROSS_COMPILE=x86_64-linux-gnu-");
                }
                _ => {
                    error!(
                        "Unsupported architecture for make: {:?}",
                        context.config.arch
                    );
                }
            }

            if context.config.toolchains.clang {
                if context.config.toolchains.llvm_ias {
                    proc.arg("LLVM=1");
                    proc.arg("LLVM_IAS=1");
                } else if context.config.toolchains.llvm_binutils {
                    proc.arg("CC=clang");
                    proc.arg("LD=ld.lld");
                    proc.arg("AR=llvm-ar");
                    proc.arg("NM=llvm-nm");
                    proc.arg("OBJCOPY=llvm-objcopy");
                    proc.arg("OBJDUMP=llvm-objdump");
                    proc.arg("STRIP=llvm-strip");
                } else {
                    proc.arg("CC=clang");
                }
            }

            for env_var in &context.config.env {
                info!(
                    "Setting environment variable for make: {}={}",
                    env_var.name, env_var.value
                );
                proc.env(env_var.name.clone(), env_var.value.clone());
            }

            // Ensure toolchain bin is in PATH
            let path = EnvVar {
                name: "PATH".into(),
                value: format!(
                    "{}:{}",
                    toolchain_dir.join("bin").to_string_lossy(),
                    std::env::var("PATH").unwrap_or_default()
                ),
            };
            proc.env(path.name, path.value);

            debug!("Running make in directory: {:?}", &context.work_dir);

            // 1. SETUP THE PROCESS
            proc.stdout(Stdio::piped());
            proc.stderr(Stdio::piped()); // Capture stderr too (make often prints info here)

            // 2. SPAWN THE PROCESS
            // We use spawn() instead of status() so we can interact with it while it runs.
            let mut child = proc
                .spawn()
                .map_err(|e| Status::internal(format!("Failed to spawn make: {}", e)))?;

            // 3. GRAB THE HANDLES
            // We take ownership of the pipes. If we don't, they close immediately.
            let stdout = child.stdout.take().expect("stdout not piped");
            let stderr = child.stderr.take().expect("stderr not piped");

            // 4. START LOG STREAMERS
            // We spawn two background tasks to read logs so they don't block each other.
            let tx_out = tx.clone();
            let tx_err = tx.clone();

            // Task A: Forward Stdout
            let out_handle = tokio::spawn(async move {
                let mut reader = BufReader::new(stdout).lines();
                while let Ok(Some(line)) = reader.next_line().await {
                    // We cannot use 'report!' here easily because of 'break',
                    // so we manually send.
                    let msg = BuildStatus {
                        status: ProgressStatus::InProgressBuild.into(),
                        output: format!("stdout: {}", line),
                        build_id: Some(req.build_id),
                    };
                    // If client disconnects, stop reading
                    if tx_out.send(Ok(msg)).await.is_err() {
                        break;
                    }
                }
            });

            // Task B: Forward Stderr (Merged into the same stream)
            let err_handle = tokio::spawn(async move {
                let mut reader = BufReader::new(stderr).lines();
                while let Ok(Some(line)) = reader.next_line().await {
                    let msg = BuildStatus {
                        status: ProgressStatus::InProgressBuild.into(),
                        output: format!("stderr: {}", line),
                        build_id: Some(req.build_id),
                    };
                    if tx_err.send(Ok(msg)).await.is_err() {
                        break;
                    }
                }
            });

            report!(InProgressBuild, "Build in progress...");

            // 5. WAIT FOR COMPLETION
            // Wait for the process to exit...
            let status_result = tokio::select! {
                // Option A: The process finishes naturally
                res = child.wait() => {
                    res
                }
                // Option B: We receive the kill signal
                _ = rx_kill.recv() => {
                    report!(Failed, "Kill signal received. Terminating build process...");

                    // Kill the process safely
                    if let Err(e) = child.kill().await {
                        error!("Failed to kill process: {}", e);
                    }

                    // Clean up logging tasks
                    let _ = out_handle.await;
                    let _ = err_handle.await;

                    report!(Failed, "Build cancelled by user.");

                    // Mark as finished locally so we stop tracking it
                     let mut per_build_statuses_lock = peridstat.lock().await;
                    if let Some(entry) = per_build_statuses_lock.iter_mut().find(|s| s.build_id == req.build_id) {
                        entry.finished = true;
                    }
                    return Ok(()); // Exit the task immediately
                }
            };
            let status = match status_result {
                Ok(s) => s,
                Err(e) => {
                    report!(Failed, format!("Failed to wait for make process: {}", e));
                    return Err(Status::internal(format!(
                        "Failed to wait for make process: {}",
                        e
                    )));
                }
            };

            // ...AND wait for the logs to finish flushing.
            // If we don't await these, we might lose the last few lines of logs.
            let _ = out_handle.await;
            let _ = err_handle.await;

            // 6. CHECK FINAL STATUS
            if !status.success() {
                report!(Failed, format!("make failed with status: {:?}", status));
                return Err(Status::internal("make failed"));
            }

            if let Some(anykernel) = &context.config.anykernel
                && anykernel.enabled
            {
                report!(InProgressBuild, "Packaging with AnyKernel...");
                if anykernel.location.is_none() {
                    report!(
                        InProgressBuild,
                        "AnyKernel packaging config is invalid, skipping."
                    );
                } else {
                    let anykernel_dir = context.work_dir.join(anykernel.location.as_ref().unwrap());
                    if !anykernel_dir.exists() {
                        report!(
                            Failed,
                            format!("AnyKernel directory {:?} does not exist.", anykernel_dir)
                        );
                        return Err(Status::internal(format!(
                            "AnyKernel directory {:?} does not exist.",
                            anykernel_dir
                        )));
                    }
                    let date = Local::now();
                    let formatted_date = format!("{}", date.format("%Y-%m-%d_%H-%M-%S"));
                    let zip_file_path = context.work_dir.join(format!(
                        "{}_{}-{}.zip",
                        context.config.name, req.build_id, formatted_date
                    ));
                    let file = File::create(&zip_file_path)?;
                    let mut zip = ZipWriter::new(file);

                    report!(
                        InProgressBuild,
                        format!("CreateZipFile {:?}", &zip_file_path)
                    );

                    let options = FileOptions::<()>::default()
                        .compression_method(CompressionMethod::Deflated);

                    for entry in walkdir::WalkDir::new(&anykernel_dir) {
                        let entry = entry.map_err(|e| {
                            Status::internal(format!(
                                "Failed to read entry in AnyKernel directory {:?}: {}",
                                anykernel_dir, e
                            ))
                        })?;
                        let path = entry.path();
                        if path.is_file() && !path.starts_with(".") {
                            let name = path
                                .strip_prefix(&anykernel_dir)
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
                                debug!("Skipping zero-length file in AnyKernel zip: {:?}", path);
                                continue;
                            }
                            zip.start_file(name, options).map_err(|e| {
                                Status::internal(format!(
                                    "Failed to add file to zip {:?}: {}",
                                    path, e
                                ))
                            })?;
                            let mut f = std::fs::File::open(&path).map_err(|e| {
                                Status::internal(format!(
                                    "Failed to open file {:?} for zipping: {}",
                                    path, e
                                ))
                            })?;
                            std::io::copy(&mut f, &mut zip).map_err(|e| {
                                Status::internal(format!(
                                    "Failed to copy file {:?} into zip: {}",
                                    path, e
                                ))
                            })?;
                            debug!("Added file to AnyKernel zip: {:?}", path);
                        }
                    }
                    zip.finish().map_err(|e| {
                        Status::internal(format!("Failed to finalize zip file: {}", e))
                    })?;
                    report!(InProgressBuild, "AnyKernel packaging complete.");
                    // Grab context
                    let mut contexts = contexts_clone.lock().await;
                    if let Some(ctx) = contexts.iter_mut().find(|c| c.id == req.build_id) {
                        ctx.artifact_path = Some(zip_file_path);
                    }
                }
            }

            report!(Success, "Build complete. Dropping build context.");

            // Mark build as finished
            let mut per_build_statuses_lock = peridstat.lock().await;
            if let Some(entry) = per_build_statuses_lock
                .iter_mut()
                .find(|s| s.build_id == req.build_id)
            {
                entry.finished = true;
            }
            Ok(())
        });
        let stream = ReceiverStream::new(rx);
        Ok(tonic::Response::new(Box::pin(stream) as Self::doBuildStream))
    }

    async fn cancel_build(
        &self,
        request: Request<BuildRequest>,
    ) -> Result<Response<BuildStatus>, Status> {
        debug!("cancel_build called");
        let req = request.into_inner();

        // 0. Check if finishied already
        let per_build_statuses = self.build_statuses.lock().await;
        if let Some(entry) = per_build_statuses
            .iter()
            .find(|s| s.build_id == req.build_id)
        {
            if entry.finished {
                return Err(Status::failed_precondition(format!(
                    "Build with ID {} is already finished.",
                    req.build_id
                )));
            }
        }
        drop(per_build_statuses);

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
                    status: ProgressStatus::Failed.into(), // or Cancelled if you have that enum
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
        debug!("get_artifact called");

        let req = request.into_inner();
        let (tx, rx) = mpsc::channel(100);
        let contexts_handle = self.contexts.clone();
        tokio::spawn(async move {
            // Find the build context
            let context = {
                let contexts = contexts_handle.lock().await;
                let ctx = contexts.iter().find(|c| c.id == req.build_id);
                match ctx {
                    Some(c) => {
                        let cloned = c.clone();
                        drop(contexts);
                        cloned
                    }
                    None => {
                        let _ = tx
                            .send(Err(Status::not_found(format!(
                                "No build context found for ID: {}",
                                req.build_id
                            ))))
                            .await;
                        return;
                    }
                }
            };
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
                return;
            }

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
        let stream = ReceiverStream::new(rx);
        Ok(tonic::Response::new(
            Box::pin(stream) as Self::getArtifactStream
        ))
    }
}
