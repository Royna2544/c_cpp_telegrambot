pub(crate) mod grpc_pb {
    include!(concat!(env!("OUT_DIR"), "/tgbot.builder.android.rs"));
}

pub use grpc_pb::rom_build_service_server::RomBuildServiceServer;

use super::{
    config_resolver::{ResolveRomConfigRequest, RomConfigResolver},
    domain::{RomBuildDirs, RomBuildRequest, RomBuildTask, RomBuildVariant, RomUploadMethod},
    harness::RomBuildHarness,
    service::{ActiveBuild, BuildEntry, BuildService, BuildStatus, BuildTask},
};
use crate::build_common::{BuildError, BuildId};
use crate::gofile_api::upload_file_to_gofile;
use futures_util::Stream;
use grpc_pb::{
    BuildAction, BuildLogEntry, BuildRequest, BuildResult, BuildSubmission, CleanDirectoryRequest,
    CleanDirectoryType, DirectoryExistsResponse, Settings, UploadMethod,
    build_result::ResultDetails, rom_build_service_server,
};
#[cfg(unix)]
use std::path::Path;
use std::pin::Pin;
use tokio::{
    io::AsyncReadExt,
    sync::{broadcast, mpsc},
};
use tokio_stream::wrappers::ReceiverStream;
use tonic::{Request, Response, Status, async_trait};
#[cfg(unix)]
use tracing::debug;
use tracing::{error, info, warn};

fn build_error_to_status(error: BuildError) -> Status {
    match error {
        BuildError::InvalidRequest(message) | BuildError::ConfigNotFound(message) => {
            Status::invalid_argument(message)
        }
        BuildError::AlreadyRunning => Status::resource_exhausted(error.to_string()),
        BuildError::NotRunning | BuildError::BuildNotFound(_) => {
            Status::not_found(error.to_string())
        }
        BuildError::Cancelled => Status::cancelled(error.to_string()),
        BuildError::MissingTool(_) => Status::failed_precondition(error.to_string()),
        BuildError::ArtifactNotFound => Status::not_found(error.to_string()),
        BuildError::CommandFailed { .. }
        | BuildError::UploadFailed(_)
        | BuildError::Io(_)
        | BuildError::Internal(_) => Status::internal(error.to_string()),
    }
}

fn rom_build_variant(value: i32) -> Result<RomBuildVariant, Status> {
    match grpc_pb::BuildVariant::try_from(value)
        .map_err(|_| Status::invalid_argument("Invalid build variant specified in request"))?
    {
        grpc_pb::BuildVariant::User => Ok(RomBuildVariant::User),
        grpc_pb::BuildVariant::UserDebug => Ok(RomBuildVariant::UserDebug),
        grpc_pb::BuildVariant::Eng => Ok(RomBuildVariant::Eng),
    }
}

fn rom_upload_method(value: i32) -> RomUploadMethod {
    match UploadMethod::try_from(value) {
        Ok(UploadMethod::None) => RomUploadMethod::None,
        Ok(UploadMethod::LocalFile) => RomUploadMethod::LocalFile,
        Ok(UploadMethod::Stream) => RomUploadMethod::Stream,
        Ok(UploadMethod::GoFile) => RomUploadMethod::GoFile,
        Err(_) => RomUploadMethod::Unknown(value),
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
        Ok(tonic::Response::new(Settings {
            do_repo_sync: Some(settings.do_repo_sync),
            do_clean_build: Some(settings.do_clean_build),
            use_ccache: Some(settings.use_ccache),
            use_rbe_service: Some(settings.use_rbe_service),
            rbe_api_token: settings.rbe_api_token.clone(),
            do_upload: Some(settings.do_upload),
        }))
    }

    async fn set_settings(
        &self,
        request: tonic::Request<Settings>,
    ) -> std::result::Result<tonic::Response<()>, tonic::Status> {
        log_who_asked_me("set_settings", &request);
        let req = request.into_inner();
        let mut settings = self.settings.lock().await;
        if let Some(value) = req.do_repo_sync {
            info!("Setting do_repo_sync to {}", value);
            settings.do_repo_sync = value;
        }
        if let Some(value) = req.do_clean_build {
            info!("Setting do_clean_build to {}", value);
            settings.do_clean_build = value;
        }
        if let Some(value) = req.use_ccache {
            info!("Setting use_ccache to {}", value);
            settings.use_ccache = value;
        }
        if let Some(value) = req.use_rbe_service {
            info!("Setting use_rbe_service to {}", value);
            settings.use_rbe_service = value;
        }
        if req.rbe_api_token.is_some() {
            info!("Setting rbe_api_token");
            settings.rbe_api_token = req.rbe_api_token;
        }
        if let Some(value) = req.do_upload {
            info!("Setting do_upload to {}", value);
            settings.do_upload = value;
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
        let mut lock = self.registry.active.lock().await;

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
        let (kill_tx, kill_rx) = mpsc::channel::<()>(1);

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

        let resolved = RomConfigResolver::new(&self.configs)
            .resolve(ResolveRomConfigRequest {
                config_name: &req.config_name,
                target_device: &req.target_device,
                rom_name: &req.rom_name,
                rom_android_version: req.rom_android_version,
            })
            .map_err(build_error_to_status)?;

        let parallel_jobs = match req.parallel_jobs {
            Some(jobs) => {
                info!("Using {} parallel jobs for build", jobs);
                jobs
            }
            None => num_cpus::get() as i32,
        };

        let build_variant = rom_build_variant(req.build_variant)?;

        let known_builds_entry = BuildEntry {
            id: build_id.clone(),
            variant: build_variant,
            target_device: resolved.device_entry.clone(),
            config_name: req.config_name.clone(),
            success: BuildStatus::InProgress,
        };
        self.registry.known.lock().await.push(known_builds_entry);

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

        let settings = self.settings.lock().await.clone();
        let log_tx_clone = log_tx.clone();
        let askpass_path_clone = askpass_path.clone();
        let runner = self.runner.clone();
        let git = self.git.clone();
        let fs = self.fs.clone();
        let request = RomBuildRequest {
            config_name: req.config_name,
            target_codename: req.target_device,
            rom_name: req.rom_name,
            rom_android_version: req.rom_android_version,
            variant: build_variant,
            force_checkout: req.force_checkout.unwrap_or(false),
            parallel_jobs,
            github_token: req.github_token,
            upload_method: rom_upload_method(req.upload_method),
        };

        let span = tracing::info_span!("build_task", build_id = build_id);
        let task = BuildTask {
            domain: RomBuildTask {
                id: BuildId::new(build_id.clone()),
                request,
                settings,
                dirs: RomBuildDirs {
                    build: self.build_dir.clone(),
                    temp: self.tempdir.clone(),
                },
                resolved,
            },
            log_tx_clone,
            registry: self.registry.clone(),
            askpass_path_clone,
            runner,
            git,
            fs,
            kill_rx,
            span,
        };
        let task_handle: tokio::task::JoinHandle<Result<(), BuildError>> =
            tokio::spawn(RomBuildHarness::run(task));

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
        let lock = self.registry.active.lock().await;

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
        let lock = self.registry.active.lock().await;

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
        let lock = self.registry.active.lock().await;

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
        let known_builds = self.registry.known.lock().await;

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

        let upload_tasks = self.registry.uploads.lock().await;
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

        let file_name = upload_task.artifact.file_name.clone();
        info!("Artifact file name: {}", &file_name);

        tokio::spawn(async move {
            match upload_task.method {
                RomUploadMethod::LocalFile => {
                    info!(
                        "Returning LocalFile build result for build ID: {}",
                        req.build_id
                    );
                    tx.send(Ok(BuildResult {
                        success: true,
                        upload_method: UploadMethod::LocalFile as i32,
                        result_details: Some(ResultDetails::LocalFilePath(
                            upload_task.artifact.path.to_string_lossy().to_string(),
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
                RomUploadMethod::Stream => {
                    info!(
                        "Returning Stream build result for build ID: {}",
                        req.build_id
                    );
                    let mut file = tokio::fs::File::open(&upload_task.artifact.path)
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
                RomUploadMethod::GoFile => {
                    info!(
                        "Returning GoFile build result for build ID: {}",
                        req.build_id
                    );
                    let upload_response =
                        upload_file_to_gofile(&upload_task.artifact.path.to_string_lossy())
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
