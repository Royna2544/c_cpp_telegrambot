mod grpc_pb {
    include!(concat!(env!("OUT_DIR"), "/tgbot.builder.android.rs"));
}

pub use crate::rombuild::build_service::grpc_pb::rom_build_service_server::RomBuildServiceServer;
use crate::rombuild::build_service::grpc_pb::{
    BuildAction, BuildLogEntry, BuildRequest, BuildSubmission, CleanDirectoryRequest,
    CleanDirectoryType, Settings, rom_build_service_server,
};

use futures_util::Stream;
use std::{path::PathBuf, pin::Pin, sync::Arc};
use tokio::sync::Mutex;
use tonic::{Request, async_trait};
use tracing::info;

pub struct BuildService {
    settings: Arc<Mutex<Settings>>,
    build_dir: PathBuf,
}

impl BuildService {
    pub fn new(build_dir: PathBuf) -> Self {
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

#[async_trait]
impl rom_build_service_server::RomBuildService for BuildService {
    type StreamLogsStream =
        Pin<Box<dyn Stream<Item = Result<BuildLogEntry, tonic::Status>> + Send + 'static>>;

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
        info!("Cleaning directory: {}", req.directory_type);
        std::fs::remove_dir_all(&clean_dir)
            .map_err(|e| tonic::Status::internal(format!("Failed to clean directory: {}", e)))?;
        Ok(tonic::Response::new(()))
    }

    /// Start a new ROM build.
    async fn start_build(
        &self,
        request: tonic::Request<BuildRequest>,
    ) -> std::result::Result<tonic::Response<BuildSubmission>, tonic::Status> {
        log_who_asked_me("start_build", &request);
        unimplemented!();
    }

    /// Logs streaming for a build in progress
    async fn stream_logs(
        &self,
        request: tonic::Request<BuildAction>,
    ) -> std::result::Result<tonic::Response<Self::StreamLogsStream>, tonic::Status> {
        log_who_asked_me("stream_logs", &request);
        unimplemented!();
    }

    /// Cancel a build in progress
    async fn cancel_build(
        &self,
        request: tonic::Request<BuildAction>,
    ) -> std::result::Result<tonic::Response<()>, tonic::Status> {
        log_who_asked_me("cancel_build", &request);
        unimplemented!();
    }

    async fn get_status(
        &self,
        request: tonic::Request<BuildAction>,
    ) -> std::result::Result<tonic::Response<BuildSubmission>, tonic::Status> {
        log_who_asked_me("get_status", &request);
        unimplemented!();
    }
}
