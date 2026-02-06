mod grpc_pb {
    include!(concat!(env!("OUT_DIR"), "/tgbot.builder.healthcheck.rs"));
}

use grpc_pb::health_check_service_server::HealthCheckService;
pub use grpc_pb::health_check_service_server::HealthCheckServiceServer;
use tracing::info;

#[derive(Default)]
pub struct HealthServiceImpl {}

impl HealthServiceImpl {
    pub fn new() -> Self {
        Self::default()
    }
}

#[tonic::async_trait]
impl HealthCheckService for HealthServiceImpl {
    async fn ping(
        &self,
        _request: tonic::Request<()>,
    ) -> Result<tonic::Response<()>, tonic::Status> {
        info!("Pong!");
        Ok(tonic::Response::new(()))
    }
}
