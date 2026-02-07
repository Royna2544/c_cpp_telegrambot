use std::path::PathBuf;

use serde::{Deserialize, Serialize};
use tracing::{error, info};

use crate::util::{self, new_impl};

#[derive(Debug, Deserialize, Serialize, Clone)]
pub enum ROMArtifactMatcher {
    ZipFilePrefixer,
    ExactMatcher,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct ROMArtifactEntry {
    pub matcher: ROMArtifactMatcher,
    pub data: String,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct ROMBranchEntry {
    pub android_version: f32,
    pub branch: String,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct ROMEntry {
    pub name: String,
    pub link: String,
    pub target: String,
    pub artifact: ROMArtifactEntry,
    pub branches: Vec<ROMBranchEntry>,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct TargetsEntry {
    pub name: String,
    pub codename: String,
    pub manufacturer: String,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct ManifestBranchesEntry {
    pub name: String,
    pub target_rom: String,
    pub android_version: f32,
    pub device: String,
    pub use_regex: bool,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct ManifestEntry {
    pub name: String,
    pub url: String,
    pub branches: Vec<ManifestBranchesEntry>,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct CloneMappingsEntry {
    pub repo: String,
    pub branch: String,
    pub path: String,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
pub struct RecoveryManifestEntry {
    pub name: String,
    pub clone_mappings: Vec<CloneMappingsEntry>,
    pub android_version: f32,
    pub target_recovery: String,
    pub device: String,
    pub use_regex: bool,
}

#[derive(Debug, Clone)]
pub struct ROMBuildConfig {
    pub roms: Vec<ROMEntry>,
    pub recoveries: Vec<ROMEntry>,
    pub targets: Vec<TargetsEntry>,
    pub manifests: Vec<ManifestEntry>,
    pub recovery_manifests: Vec<RecoveryManifestEntry>,
}

impl ROMEntry {
    pub fn new(file_path: &PathBuf) -> Result<Vec<ROMEntry>, ()> {
        new_impl(file_path)
    }
}

impl TargetsEntry {
    pub fn new(file_path: &PathBuf) -> Result<Vec<TargetsEntry>, ()> {
        new_impl(file_path)
    }
}

impl ManifestEntry {
    pub fn new(file_path: &PathBuf) -> Result<ManifestEntry, ()> {
        new_impl(file_path)
    }
}

impl RecoveryManifestEntry {
    pub fn new(file_path: &PathBuf) -> Result<RecoveryManifestEntry, ()> {
        new_impl(file_path)
    }
}

impl ROMBuildConfig {
    pub fn new(json_dir: &PathBuf) -> Option<ROMBuildConfig> {
        let target_file = json_dir.join("targets.json");
        let manifest_file = json_dir.join("roms.json");
        let recovery_file = json_dir.join("recoveries.json");

        let targets = match TargetsEntry::new(&target_file) {
            Ok(cfg) => cfg,
            Err(_) => {
                error!("Failed to parse targets config from file {:?}", target_file);
                Vec::new()
            }
        };

        let roms = match ROMEntry::new(&manifest_file) {
            Ok(cfg) => cfg,
            Err(_) => {
                error!("Failed to parse ROMs config from file {:?}", manifest_file);
                Vec::new()
            }
        };

        let recoveries = match ROMEntry::new(&recovery_file) {
            Ok(cfg) => cfg,
            Err(_) => {
                error!(
                    "Failed to parse recoveries config from file {:?}",
                    recovery_file
                );
                Vec::new()
            }
        };

        let manifests =
            util::for_each_json_file(&json_dir.join("manifest"), |path| match ManifestEntry::new(
                path,
            ) {
                Ok(manifest) => Ok(manifest),
                Err(_) => {
                    error!("Failed to parse manifest config from file {:?}", path);
                    Err(())
                }
            });

        let recovery_manifests =
            util::for_each_json_file(&json_dir.join("manifest").join("recovery"), |path| {
                match RecoveryManifestEntry::new(path) {
                    Ok(manifest) => Ok(manifest),
                    Err(_) => {
                        error!(
                            "Failed to parse recovery manifest config from file {:?}",
                            path
                        );
                        Err(())
                    }
                }
            });

        info!(
            "Loaded Android ROM Build configuration with {} ROM entries and {} target entries.",
            roms.len(),
            targets.len()
        );

        Some(ROMBuildConfig {
            roms,
            targets,
            manifests,
            recoveries,
            recovery_manifests,
        })
    }
}
