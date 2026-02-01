use crate::builder_config::Architecture;
use serde::{Deserialize, Serialize};

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct Repo {
    pub url: String,
    pub branch: String,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct AnyKernel {
    pub enabled: bool,
    pub location: Option<String>,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct Defconfig {
    pub scheme: String,
    pub devices: Vec<String>,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct Fragment {
    pub name: String,
    pub scheme: String,
    #[serde(default)]
    pub depends: Vec<String>,
    pub description: Option<String>,
    pub default_enabled: bool,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct EnvVar {
    pub name: String,
    pub value: String,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct LLVMSupport {
    pub clang: bool,
    pub llvm_binutils: bool,
    pub llvm_ias: bool,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct KernelConfig {
    pub name: String,

    // Nested struct
    pub repo: Repo,

    // Simple strings
    pub arch: Architecture,

    // "type" is a reserved keyword in Rust, so we rename it
    #[serde(rename = "type")]
    pub image_type: String,

    pub toolchains: LLVMSupport,

    // Optional
    pub anykernel: Option<AnyKernel>,

    pub defconfig: Defconfig,

    #[serde(default)]
    pub fragments: Vec<Fragment>,

    #[serde(default)]
    pub env: Vec<EnvVar>,
}
