use std::path::PathBuf;

use tokio::sync::mpsc;

use super::{builder_config::Toolchain, kernel_config::KernelConfig};

#[derive(Clone)]
pub struct BuildContext {
    pub(crate) id: i32,
    pub(crate) config: KernelConfig,
    pub(crate) toolchain: Toolchain,
    pub(crate) work_dir: PathBuf,
    pub(crate) toolchain_dir: PathBuf,
    pub(crate) device_name: String,
    pub(crate) artifact_path: Option<PathBuf>,
    pub(crate) kill_signal: Option<mpsc::Sender<()>>,
}

pub(crate) struct PerBuildIdStatus {
    pub(crate) build_id: i32,
    pub(crate) finished: bool,
    pub(crate) succeeded: bool,
}
