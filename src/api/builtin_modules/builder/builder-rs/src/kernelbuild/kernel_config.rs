use std::path::PathBuf;

use crate::{kernelbuild::builder_config::Architecture, util::new_impl};
use serde::{Deserialize, Serialize};
use tracing::error;

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
    #[serde(default)]
    pub llvm_binutils: bool,
    #[serde(default)]
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

impl KernelConfig {
    pub fn supports_arch(&self, arch: &Architecture) -> bool {
        self.arch == *arch || self.arch == Architecture::Any
    }

    // Construct build arguments based on the kernel config
    pub fn build_args(&self) -> Vec<String> {
        let mut args: Vec<String> = Vec::new();

        if !self.supports_arch(&self.arch) {
            error!(
                "KernelConfig {} does not support architecture {:?}",
                self.name, self.arch
            );
            return args;
        }

        // Set architecture
        args.push(format!(
            "ARCH={}",
            match self.arch {
                Architecture::ARM => "arm",
                Architecture::ARM64 => "arm64",
                Architecture::X86 => "x86",
                Architecture::X86_64 => "x86_64",
                Architecture::Any => "",
            }
        ));

        // Set compiler options (LLVM/Clang)
        if self.toolchains.clang {
            if self.toolchains.llvm_ias {
                args.push("LLVM=1".to_string());
                args.push("LLVM_IAS=1".to_string());
            } else if self.toolchains.llvm_binutils {
                args.push("CC=clang".to_string());
                args.push("LD=ld.lld".to_string());
                args.push("AR=llvm-ar".to_string());
                args.push("NM=llvm-nm".to_string());
                args.push("OBJCOPY=llvm-objcopy".to_string());
                args.push("OBJDUMP=llvm-objdump".to_string());
                args.push("STRIP=llvm-strip".to_string());
            } else {
                args.push("CC=clang".to_string());
            }
        }

        args
    }

    pub fn env_vars(&self, toolchain_dir: PathBuf) -> Vec<(String, String)> {
        let path = EnvVar {
            name: "PATH".into(),
            value: format!(
                "{}:{}",
                toolchain_dir.join("bin").to_string_lossy(),
                std::env::var("PATH").unwrap_or_default()
            ),
        };
        let build_user = EnvVar {
            name: "KBUILD_BUILD_USER".into(),
            value: "builder-rs".into(),
        };
        let build_host = EnvVar {
            name: "KBUILD_BUILD_HOST".into(),
            value: "builder-rs-host".into(),
        };
        self.env
            .iter()
            .map(|var| (var.name.clone(), var.value.clone()))
            .into_iter()
            .chain(std::iter::once((path.name, path.value)))
            .chain(std::iter::once((build_user.name, build_user.value)))
            .chain(std::iter::once((build_host.name, build_host.value)))
            .collect()
    }

    pub fn new(file_path: &PathBuf) -> Result<KernelConfig, ()> {
        new_impl(&file_path)
    }
}
