use std::fmt::format;
use std::path::Path;
use std::path::PathBuf;
use std::time::Duration;
use tokio::io::AsyncReadExt;

use serde::Deserialize;
use serde::Serialize;
use tar::Archive;
use tokio::fs::File;
use tokio::io::AsyncWriteExt;
use tokio::time::Instant;
use tokio_stream::StreamExt;
use tonic::Status;
use tracing::debug;
use tracing::error;
use tracing::info;

use crate::git_repo::GitRepo;
use crate::util::new_impl;
use tokio::fs::File as TokioFile;

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

    async fn download_file<F, Fut>(
        url: &str,
        dest: &Path,
        progress: F,
    ) -> Result<(), Box<dyn std::error::Error>>
    where
        F: Fn(u64, u64) -> Fut,
        Fut: Future<Output = ()>,
    {
        let response = reqwest::get(url).await?;

        let mut downloaded_size = 0;

        // 1. Create the file
        let mut file = TokioFile::create(dest).await?;

        // 2. Stream the content
        let mut stream = response.bytes_stream();

        let mut last_update = Instant::now();
        let update_interval = Duration::from_secs(5);

        while let Some(item) = stream.next().await {
            let chunk = item?;
            if last_update.elapsed() >= update_interval || downloaded_size == 0 {
                progress(chunk.len() as u64, downloaded_size).await;
                last_update = Instant::now();
            }
            downloaded_size += chunk.len() as u64;
            file.write_all(&chunk).await?;
        }

        Ok(())
    }

    fn extract_tar_gz(
        tar_gz_path: &Path,
        dest_dir: &Path,
    ) -> Result<(), Box<dyn std::error::Error>> {
        debug!(
            "Extracting tarball {} to {}",
            tar_gz_path.display(),
            dest_dir.display()
        );
        let tar_gz = std::fs::File::open(tar_gz_path)?;
        let decompressor = flate2::read::GzDecoder::new(tar_gz);
        let mut archive = Archive::new(decompressor);
        archive.unpack(dest_dir)?;
        debug!(
            "Extracted tarball {} to {}",
            tar_gz_path.display(),
            dest_dir.display()
        );
        Ok(())
    }

    pub async fn clone_to_dir(&self, dest_path: &PathBuf) -> Result<(), Status> {
        if std::path::Path::new(&dest_path).exists() {
        } else {
            if let Err(e) = std::fs::create_dir_all(&dest_path) {
                info!("Failed to create directory {}: {}", dest_path.display(), e);
            }
        }
        match &self.source_type {
            Source::Git => {
                info!(
                    "Cloning toolchain {} from {} with branch {:?}",
                    self.name, self.url, self.branch
                );
                match GitRepo::clone(
                    &self.url,
                    self.branch.as_deref().unwrap_or("master"),
                    Some(1),
                    dest_path,
                    None,
                    &None,
                ) {
                    Ok(_) => {
                        info!(
                            "Successfully cloned toolchain {} into {}",
                            self.name,
                            dest_path.display()
                        );
                        Ok(())
                    }
                    Err(e) => {
                        return Err(Status::internal(format!(
                            "Failed to initialize git repo: {}",
                            e
                        )));
                    }
                }
            }
            Source::Tarball => {
                info!("Downloading toolchain {} from {}", self.name, self.url);
                let dest_file = dest_path.join(format!("{}.tar.gz", &self.name));
                Self::download_file(&self.url, &dest_file, async |current, total| {
                    info!(
                        "Downloading toolchain... Total downloaded {} KB",
                        total / 1024
                    );
                })
                .await
                .map_err(|e| Status::internal(format!("Download failed: {}", e)))?;

                Self::extract_tar_gz(&dest_file, &dest_path)
                    .map_err(|e| Status::internal(format!("Extraction failed: {}", e)))?;
                info!(
                    "Successfully downloaded and extracted toolchain {}",
                    self.name
                );
                Ok(())
            }
        }
    }
}

impl BuilderConfig {
    pub fn new(json_file: &PathBuf) -> Result<BuilderConfig, ()> {
        new_impl(json_file)
    }
}
