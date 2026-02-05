use crate::kernelbuild::build_service::linux_kernel_build_service_server::LinuxKernelBuildServiceServer;
use crate::kernelbuild::builder_config::BuilderConfig;
use crate::kernelbuild::kernel_config::KernelConfig;
use crate::rombuild::build_service::BuildService as ROMBuildService;
use crate::rombuild::types::ROMBuildConfig;
use crate::system_monitor::grpc_monitor::system_monitor_service_server::SystemMonitorServiceServer;
use crate::{
    health::HealthCheckServiceServer, kernelbuild::build_service::BuildService,
    rombuild::build_service::RomBuildServiceServer,
};
use clap::Parser;
use std::path::PathBuf;
use tonic::transport::Server;
use tracing::{debug, error, info, warn};
use tracing_subscriber::EnvFilter;

const FILE_DESCRIPTOR_SET: &[u8] = tonic::include_file_descriptor_set!("descriptor");

mod git_repo;
mod health;
mod kernelbuild;
mod ratelimit;
mod rombuild;
mod system_monitor;
mod util;

#[derive(Parser, Debug)]
#[command(version, about, long_about = None)]
struct KernelBuilderArgs {
    #[arg(long)]
    bind_addr: String,
    #[arg(long)]
    temp_dir: String,
    #[arg(long)]
    kernelbuild_json_dir: String,
    #[arg(long)]
    kernelbuild_output_dir: String,
    #[arg(long)]
    rombuild_json_dir: String,
    #[arg(long)]
    rombuild_output_dir: String,
}

fn make_kernel_builder_service(
    kernelbuild_output_dir: &PathBuf,
    kernelbuild_json_dir: &PathBuf,
    temp_dir: &PathBuf,
) -> Option<BuildService> {
    // Load Kernel Configurations
    let configs = util::for_each_json_file(&PathBuf::from(&kernelbuild_json_dir), move |path| {
        match KernelConfig::new(&path) {
            Ok(cfg) => Ok(cfg),
            Err(_) => {
                error!("Failed to parse kernel config from file {:?}", path);
                Err(())
            }
        }
    });

    // Check if any configurations were loaded
    if configs.is_empty() {
        warn!("No valid kernel configurations found in the specified directory.");
        warn!("Exiting due to lack of configurations.");
        return None;
    } else {
        info!("Loaded {} kernel configurations.", configs.len());
    }

    // Load Builder Configuration (incl. toolchain configs)
    let builder_config: Result<BuilderConfig, ()> =
        match BuilderConfig::new(&PathBuf::from(&kernelbuild_json_dir).join("builder_config.json"))
        {
            Ok(builder_config_val) => {
                info!("Loaded builder configuration.");
                Ok(builder_config_val)
            }
            Err(_) => {
                error!("Failed to load builder configuration. Exiting.");
                Err(())
            }
        };
    let builder_config = builder_config.ok()?;

    debug!(
        "BuildService::BuildService - Loaded {} toolchain configurations.",
        builder_config.toolchains.len()
    );

    // Initialize Build Service
    Some(BuildService::new(
        configs,
        builder_config,
        temp_dir.to_path_buf(),
        kernelbuild_output_dir.to_path_buf(),
    ))
}

fn make_rom_build_service(
    rombuild_json_dir: &PathBuf,
    rombuild_output_dir: PathBuf,
) -> Option<ROMBuildService> {
    let rombuild_config: Option<ROMBuildConfig> =
        match ROMBuildConfig::new(&PathBuf::from(&rombuild_json_dir)) {
            Some(rombuild_config) => Some(rombuild_config),
            None => {
                warn!("No valid Android ROM build configuration found.");
                warn!("Exiting due to lack of configurations.");
                None
            }
        };

    Some(ROMBuildService::new(rombuild_output_dir, rombuild_config?))
}

#[tokio::main]
async fn main() {
    tracing_subscriber::fmt()
        .with_file(true)
        .with_line_number(true)
        .with_target(false)
        .with_env_filter(EnvFilter::from_default_env())
        .init();

    let args = KernelBuilderArgs::parse();

    info!("Starting Linux Kernel+Android ROM Builder Service");
    info!("Bind Address: {}", args.bind_addr);
    info!("KernelBuild JSON Directory: {}", args.kernelbuild_json_dir);
    info!(
        "KernelBuild Output Directory: {}",
        args.kernelbuild_output_dir
    );
    info!("ROMBuild JSON Directory: {}", args.rombuild_json_dir);
    info!("ROMBuild Output Directory: {}", args.rombuild_output_dir);
    info!("Temp Directory: {}", args.temp_dir);

    // 1. Define the address to listen on
    let addr = args.bind_addr.parse().expect("Invalid bind address");
    info!("Linux Kernel+Android ROM Builder listening on {}", addr);

    let temp_dir = PathBuf::from(&args.temp_dir);
    let output_dir = PathBuf::from(&args.kernelbuild_output_dir);
    let rombuild_output_dir = PathBuf::from(&args.rombuild_output_dir);
    let canonical_temp: PathBuf;
    let canonical_output: PathBuf;
    let canonical_rombuild_output: PathBuf;

    match util::make_canonical_path(&temp_dir) {
        Some(t) => {
            info!("Using canonical temp directory: {:?}", t);
            canonical_temp = t;
        }
        None => {
            return;
        }
    }

    match util::make_canonical_path_mkdirs(&output_dir) {
        Some(o) => {
            info!("Using canonical output directory: {:?}", o);
            canonical_output = o;
        }
        None => {
            return;
        }
    }

    match util::make_canonical_path_mkdirs(&rombuild_output_dir) {
        Some(r) => {
            info!("Using canonical ROM build output directory: {:?}", r);
            canonical_rombuild_output = r;
        }
        None => {
            return;
        }
    }

    // 2. Initialize Kernel Build Service
    let build_service = make_kernel_builder_service(
        &canonical_output,
        &PathBuf::from(&args.kernelbuild_json_dir),
        &canonical_temp,
    )
    .expect("Failed to initialize Kernel Build Service");

    // 2. Initialize Android ROM Build Service
    let android_build_service = make_rom_build_service(
        &PathBuf::from(&args.rombuild_json_dir),
        canonical_rombuild_output,
    )
    .expect("Failed to initialize ROM Build Service");

    // 3. Initialize System Monitor Service
    let system_monitor = system_monitor::MonitorService::new();

    // 4. Initialize Health Check Service
    let health_service = health::HealthServiceImpl::new();

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
        .add_service(HealthCheckServiceServer::new(health_service))
        .add_service(SystemMonitorServiceServer::new(system_monitor))
        .serve(addr)
        .await
        .unwrap();
}
