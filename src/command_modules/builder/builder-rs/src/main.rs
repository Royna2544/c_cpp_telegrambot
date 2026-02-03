use crate::kernelbuild::build_service::linux_kernel_build_service_server::LinuxKernelBuildServiceServer;
use crate::kernelbuild::kernel_config::KernelConfig;
use crate::rombuild::build_service::BuildService as ROMBuildService;
use crate::system_monitor::grpc_monitor::system_monitor_service_server::SystemMonitorServiceServer;
use crate::{
    kernelbuild::build_service::BuildService, rombuild::build_service::RomBuildServiceServer,
};
use clap::Parser;
use std::path::PathBuf;
use tonic::transport::Server;
use tracing::{debug, error, info, warn};

const FILE_DESCRIPTOR_SET: &[u8] = tonic::include_file_descriptor_set!("descriptor");

mod git_repo;
mod kernelbuild;
mod ratelimit;
mod rombuild;
mod system_monitor;

#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
struct KernelBuilderArgs {
    #[arg(long)]
    bind_addr: String,
    #[arg(long)]
    json_dir: String,
    #[arg(long)]
    output_dir: String,
    #[arg(long)]
    temp_dir: String,
}

// builder_config.json is a reserved filename for internal builder configs
fn parse_kernel_config(json_dir: &str) -> Vec<KernelConfig> {
    let mut configs: Vec<KernelConfig> = Vec::new();
    let entries: std::fs::ReadDir;
    match std::fs::read_dir(&json_dir) {
        Err(e) => {
            error!(
                "Failed to read JSON directory {}: {}",
                json_dir,
                e.to_string()
            );
            return configs;
        }
        Ok(value) => entries = value,
    }
    for entry in entries {
        let entry = match entry {
            Err(e) => {
                error!("Failed to read directory entry: {}", e);
                continue;
            }
            Ok(val) => val,
        };
        let path = entry.path();
        if path.extension().and_then(|s| s.to_str()) == Some("json") {
            info!("Processing file: {:?}", path);
            let file = std::fs::File::open(&path).expect("Failed to open JSON file");
            let reader = std::io::BufReader::new(file);
            match path.file_name().and_then(|s| s.to_str()) {
                Some("builder_config.json") => {
                    debug!("Skipping reserved builder_config.json file");
                    continue;
                }
                _ => {}
            }
            let json: KernelConfig = match serde_json::from_reader(reader) {
                Ok(val) => {
                    info!("Successfully parsed JSON file {:?}", path);
                    val
                }
                Err(e) => {
                    error!("Failed to parse JSON file {:?}: {}", path, e);
                    continue;
                }
            };
            configs.push(json);
        }
    }
    configs
}

fn parse_builder_config(
    json_directory: &str,
) -> Option<crate::kernelbuild::builder_config::BuilderConfig> {
    let path = std::path::Path::new(json_directory).join("builder_config.json");
    let file = std::fs::File::open(&path).ok()?;
    let reader = std::io::BufReader::new(file);
    match serde_json::from_reader(reader) {
        Err(e) => {
            error!("Failed to parse builder_config.json: {}", e);
            return None;
        }
        Ok(config) => {
            info!("Successfully parsed builder_config.json");
            Some(config)
        }
    }
}

#[tokio::main]
async fn main() {
    tracing_subscriber::fmt()
        .with_file(true)
        .with_line_number(true)
        .with_target(false)
        .init();

    let args = KernelBuilderArgs::parse();

    info!("Starting Linux Kernel Builder Service");
    info!("Bind Address: {}", args.bind_addr);
    info!("JSON Directory: {}", args.json_dir);
    info!("Output Directory: {}", args.output_dir);
    info!("Temp Directory: {}", args.temp_dir);

    let configs = parse_kernel_config(&args.json_dir);
    if configs.is_empty() {
        warn!("No valid kernel configurations found in the specified directory.");
        warn!("Exiting due to lack of configurations.");
        return;
    } else {
        info!("Loaded {} kernel configurations.", configs.len());
    }
    match parse_builder_config(&args.json_dir) {
        Some(builder_config) => {
            // 1. Define the address to listen on
            let addr = args.bind_addr.parse().expect("Invalid bind address");
            info!("Linux Kernel Builder listening on {}", addr);

            let temp_dir = PathBuf::from(&args.temp_dir);
            let output_dir = PathBuf::from(&args.output_dir);
            let canonical_temp: PathBuf;
            let canonical_output: PathBuf;

            match std::fs::canonicalize(temp_dir) {
                Ok(t) => {
                    info!("Using canonical temp directory: {:?}", t);
                    canonical_temp = t;
                }
                Err(e) => {
                    error!("Exiting due to invalid temp directory. Error: {}", e);
                    return;
                }
            }

            match std::fs::canonicalize(output_dir) {
                Ok(t) => {
                    info!("Using canonical output directory: {:?}", t);
                    canonical_output = t;
                }
                Err(e) => {
                    warn!(
                        "Output directory {:?} does not exist. Attempting to create it.",
                        args.output_dir
                    );
                    match std::fs::create_dir_all(&args.output_dir) {
                        Ok(_) => {
                            info!("Created output directory: {}", args.output_dir.as_str());
                            canonical_output = std::fs::canonicalize(&args.output_dir).unwrap();
                        }
                        Err(e) => {
                            error!(
                                "Failed to create output directory {}: {}",
                                args.output_dir.as_str(),
                                e
                            );
                            error!("Exiting due to invalid output directory.");
                            return;
                        }
                    }
                }
            }

            // 2. Initialize Build Service
            let build_service =
                BuildService::new(configs, builder_config, canonical_temp, canonical_output);
            let android_build_service = ROMBuildService::new();

            // 3. Initialize System Monitor Service
            let system_monitor = system_monitor::MonitorService::new();
            let monitor_service = SystemMonitorServiceServer::new(system_monitor);

            // 3. Build and Run the Server
            Server::builder()
                .add_service(
                    tonic_reflection::server::Builder::configure()
                        .register_encoded_file_descriptor_set(FILE_DESCRIPTOR_SET)
                        .build_v1()
                        .expect("Failed to build reflection service"),
                )
                .add_service(LinuxKernelBuildServiceServer::new(build_service))
                .add_service(RomBuildServiceServer::new(android_build_service))
                .add_service(monitor_service)
                .serve(addr)
                .await
                .unwrap();
        }
        None => {
            error!("Failed to load builder configuration. Exiting.");
        }
    }
}
