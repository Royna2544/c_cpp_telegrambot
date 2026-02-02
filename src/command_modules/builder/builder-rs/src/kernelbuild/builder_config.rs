use std::fmt::format;
use std::path::PathBuf;

use serde::Deserialize;
use serde::Serialize;
use tracing::debug;
use tracing::error;

#[derive(Debug, Deserialize, Serialize, PartialEq, Clone)]
#[serde(rename_all = "lowercase")]
pub enum CompilerType {
    GCC,
    Clang,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
#[serde(rename_all = "lowercase")]
pub enum Architecture {
    Any,
    ARM,
    ARM64,
    X86,
    X86_64,
}

impl std::fmt::Display for Architecture {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        let arch_str = match self {
            Architecture::Any => "any",
            Architecture::ARM => "arm",
            Architecture::ARM64 => "arm64",
            Architecture::X86 => "x86",
            Architecture::X86_64 => "x86_64",
        };
        write!(f, "{}", arch_str)
    }
}

#[derive(Debug, Deserialize, Serialize, PartialEq, Clone)]
#[serde(rename_all = "lowercase")]
pub enum Source {
    Git,
    Tarball,
}

impl PartialEq for Architecture {
    fn eq(&self, other: &Self) -> bool {
        matches!(
            (self, other),
            (Architecture::Any, _)
                | (_, Architecture::Any)
                | (Architecture::ARM, Architecture::ARM)
                | (Architecture::ARM64, Architecture::ARM64)
                | (Architecture::X86, Architecture::X86)
                | (Architecture::X86_64, Architecture::X86_64)
        )
    }
}

#[derive(Debug, Deserialize, PartialEq, Clone)]
pub struct Toolchain {
    pub compiler: CompilerType,
    pub compiler_version: f32,
    pub compiler_triple: Option<String>,
    pub name: String,
    pub arch: Architecture,
    #[serde(rename = "type")]
    pub source_type: Source,
    pub url: String,
    pub branch: Option<String>,
}

#[derive(Debug, Deserialize, Clone)]
pub struct BuilderConfig {
    pub toolchains: Vec<Toolchain>,
}

impl BuilderConfig {
    pub fn get_toolchains_for_arch(&self, arch: &Architecture) -> Vec<&Toolchain> {
        self.toolchains
            .iter()
            .filter(|tc| &tc.arch == arch)
            .collect()
    }
}

impl Toolchain {
    pub fn exe_name(&self) -> String {
        match self.compiler {
            CompilerType::GCC => {
                format!("{}-gcc", self.compiler_triple.as_deref().unwrap_or(""))
            }
            CompilerType::Clang => "clang".to_string(),
        }
    }

    pub fn exec_and_get_version(&self, path: &PathBuf) -> Option<String> {
        debug!("Getting version for toolchain: {:?}", self.name);
        let output = match std::process::Command::new(path.join("bin").join(self.exe_name()))
            .arg("--version")
            .output()
        {
            Ok(output) => output,
            Err(e) => {
                error!("Failed to execute compiler to get version: {}", e);
                return None;
            }
        };

        let version_str = String::from_utf8_lossy(&output.stdout);

        debug!("Version output: {}", version_str);
        let first_line = version_str.lines().next()?;
        debug!("First line of version output: {}", first_line);
        Some(first_line.to_string())
    }

    pub fn build_args(&self, arch: &Architecture) -> Vec<String> {
        let mut args = Vec::new();

        let cross_compile_default = match arch {
            Architecture::ARM => "arm-linux-gnueabi-",
            Architecture::ARM64 => "aarch64-linux-gnu-",
            Architecture::X86 => "x86_64-linux-gnu-",
            Architecture::X86_64 => "x86_64-linux-gnu-",
            _ => "",
        };
        if let Some(triple) = &self.compiler_triple {
            args.push(format!("CROSS_COMPILE={}{}", triple, "-"));
        } else if !cross_compile_default.is_empty() {
            args.push(format!("CROSS_COMPILE={}", cross_compile_default));
        }
        args
    }
}
