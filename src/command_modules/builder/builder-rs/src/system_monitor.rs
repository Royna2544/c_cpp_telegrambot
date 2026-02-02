use std::pin::Pin;
use std::sync::Arc;
use std::time::Duration;
use sysinfo::{Disks, System};
use tokio::sync::Mutex;
use tokio::time::interval;
use tokio_stream::Stream;
use tokio_stream::wrappers::ReceiverStream;
use tonic::{Request, Response, Status};

// Import generated proto code
pub mod grpc_monitor {
    include!(concat!(env!("OUT_DIR"), "/tgbot.builder.system_monitor.rs"));
}
use grpc_monitor::{GetStatsRequest, SystemInfo, WatchStatsRequest};
use grpc_monitor::{SystemStats, system_monitor_service_server::SystemMonitorService};

pub struct MonitorService {
    // System is not thread-safe for writing (refreshing), so we wrap it.
    sys: Arc<Mutex<System>>,
}

impl MonitorService {
    pub fn new() -> Self {
        let mut sys = System::new_all();
        // First refresh to establish baseline CPU
        sys.refresh_cpu();
        sys.refresh_memory();

        MonitorService {
            sys: Arc::new(Mutex::new(sys)),
        }
    }

    // Helper to extract stats struct
    async fn collect_stats(sys: &mut System) -> SystemStats {
        // Refresh only what we need
        sys.refresh_cpu();
        sys.refresh_memory();

        let cpu_usage = sys.global_cpu_info().cpu_usage();
        let used_mem = sys.used_memory() / 1024 / 1024;
        let uptime = sysinfo::System::uptime();
        let total_mem = sys.total_memory() / 1024 / 1024;

        SystemStats {
            cpu_usage_percent: cpu_usage,
            memory_used_mb: used_mem,
            memory_total_mb: total_mem,
            uptime_seconds: uptime,
        }
    }

    async fn collect_info(sys: &System, disk_path: Option<&str>) -> SystemInfo {
        let os_name = System::name().unwrap_or_default();
        let os_version = System::os_version().unwrap_or_default();
        let kernel_version = System::kernel_version().unwrap_or_default();
        let cpu_name = sys.global_cpu_info().brand().to_string();
        let cpu_cores = sys.cpus().len() as u32;
        let total_mem = sys.total_memory() / 1024 / 1024;
        let hostname = System::host_name().unwrap_or_default();
        let (disk_total, disk_used) = if let Some(path) = disk_path {
            if let Some(disk) = Disks::new_with_refreshed_list()
                .iter()
                .find(|d| path.starts_with(d.mount_point().to_str().unwrap_or("")))
            {
                let total_gb = (disk.total_space() / 1024 / 1024 / 1024) as i32;
                let used_gb =
                    ((disk.total_space() - disk.available_space()) / 1024 / 1024 / 1024) as i32;
                (Some(total_gb), Some(used_gb))
            } else {
                (None, None)
            }
        } else {
            (None, None)
        };

        SystemInfo {
            os_name,
            os_version,
            kernel_version,
            cpu_name,
            cpu_cores,
            memory_total_mb: total_mem,
            hostname,
            disk_total_gb: disk_total,
            disk_used_gb: disk_used,
        }
    }
}

#[tonic::async_trait]
impl SystemMonitorService for MonitorService {
    type WatchStatsStream =
        Pin<Box<dyn Stream<Item = Result<SystemStats, Status>> + Send + 'static>>;

    async fn get_stats(
        &self,
        _request: Request<GetStatsRequest>,
    ) -> Result<Response<SystemStats>, Status> {
        let mut sys = self.sys.lock().await;
        let stats = Self::collect_stats(&mut sys).await;
        Ok(Response::new(stats))
    }

    async fn watch_stats(
        &self,
        _request: Request<WatchStatsRequest>,
    ) -> Result<Response<Self::WatchStatsStream>, Status> {
        let (tx, rx) = tokio::sync::mpsc::channel(4);
        let sys_handle = self.sys.clone();

        tokio::spawn(async move {
            let mut ticker = interval(Duration::from_secs(
                _request.into_inner().interval_seconds.unwrap_or(2).into(),
            ));

            loop {
                ticker.tick().await;

                // Lock, refresh, extract
                let mut sys = sys_handle.lock().await;
                let stats = Self::collect_stats(&mut sys).await;
                drop(sys); // Release lock immediately

                // Send to client
                if tx.send(Ok(stats)).await.is_err() {
                    break; // Client disconnected
                }
            }
        });

        Ok(Response::new(Box::pin(ReceiverStream::new(rx))))
    }

    async fn get_system_info(&self, _request: Request<()>) -> Result<Response<SystemInfo>, Status> {
        let sys = self.sys.lock().await;
        let info = Self::collect_info(&sys, None).await;
        Ok(Response::new(info))
    }
}
