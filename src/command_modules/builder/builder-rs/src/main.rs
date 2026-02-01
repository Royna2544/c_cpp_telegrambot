use clap::Parser;
use std::sync::Arc;
use tokio::sync::Mutex;
use tonic::transport::Server;
mod build_service;
mod builder_config;
mod kernel_config;
use crate::build_service::linux_kernel_build_service_server::LinuxKernelBuildServiceServer;
use crate::build_service::{BuildService, FILE_DESCRIPTOR_SET};
use kernel_config::KernelConfig;
use std::path::PathBuf;
use tracing::{debug, error, info, instrument, warn};

#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
struct KernelBuilderArgs {
    bind_address: String,
    json_directory: String,
    output_directory: String,
    temp_directory: String,
}

// builder_config.json is a reserved filename for internal builder configs
pub fn parse_kernel_config(json_dir: &str) -> Vec<KernelConfig> {
    let mut configs: Vec<KernelConfig> = Vec::new();
    let entries = std::fs::read_dir(&json_dir).expect("Failed to read JSON directory");
    for entry in entries {
        let entry = entry.expect("Failed to read directory entry");
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

pub fn parse_builder_config(json_directory: &str) -> Option<builder_config::BuilderConfig> {
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
    tracing_subscriber::fmt::init();

    let args = KernelBuilderArgs::parse();

    info!("Starting Linux Kernel Builder Service");
    info!("Bind Address: {}", args.bind_address);
    info!("JSON Directory: {}", args.json_directory);
    info!("Output Directory: {}", args.output_directory);
    info!("Temp Directory: {}", args.temp_directory);

    let configs = parse_kernel_config(&args.json_directory);
    if configs.is_empty() {
        warn!("No valid kernel configurations found in the specified directory.");
        warn!("Exiting due to lack of configurations.");
        return;
    } else {
        info!("Loaded {} kernel configurations.", configs.len());
    }
    match parse_builder_config(&args.json_directory) {
        Some(builder_config) => {
            // 1. Define the address to listen on
            let addr = "0.0.0.0:50051".parse().unwrap();
            info!("Linux Kernel Builder listening on {}", addr);

            // 2. Initialize Build Service
            let build_service = BuildService::new(
                configs,
                builder_config,
                PathBuf::from(args.temp_directory),
                PathBuf::from(args.output_directory),
            );

            // 3. Build and Run the Server
            Server::builder()
                .add_service(
                    tonic_reflection::server::Builder::configure()
                        .register_encoded_file_descriptor_set(FILE_DESCRIPTOR_SET)
                        .build_v1()
                        .expect("Failed to build reflection service"),
                )
                .add_service(LinuxKernelBuildServiceServer::new(build_service))
                .serve(addr)
                .await
                .unwrap();
        }
        None => {
            error!("Failed to load builder configuration. Exiting.");
        }
    }
}
