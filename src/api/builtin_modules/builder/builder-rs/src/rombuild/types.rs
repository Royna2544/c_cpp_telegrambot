use std::path::PathBuf;

use serde::{Deserialize, Serialize, de::DeserializeOwned};
use tracing::info;

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

#[derive(Debug)]
pub enum ConfigError {
    Io { path: PathBuf, message: String },
    Parse { path: PathBuf, message: String },
    Validation(String),
}

impl std::fmt::Display for ConfigError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::Io { path, message } => {
                write!(f, "Failed to read {}: {message}", path.display())
            }
            Self::Parse { path, message } => {
                write!(f, "Failed to parse {}: {message}", path.display())
            }
            Self::Validation(message) => write!(f, "Invalid ROM build configuration: {message}"),
        }
    }
}

impl std::error::Error for ConfigError {}

impl ROMBuildConfig {
    pub fn load(json_dir: &PathBuf) -> Result<Self, ConfigError> {
        let config = Self {
            targets: load_json(&json_dir.join("targets.json"))?,
            roms: load_json(&json_dir.join("roms.json"))?,
            recoveries: load_json(&json_dir.join("recoveries.json"))?,
            manifests: load_json_directory(&json_dir.join("manifest"))?,
            recovery_manifests: load_json_directory(&json_dir.join("manifest").join("recovery"))?,
        };
        config.validate()?;
        info!(
            "Loaded Android ROM Build configuration with {} ROM entries and {} target entries.",
            config.roms.len(),
            config.targets.len()
        );
        Ok(config)
    }

    fn validate(&self) -> Result<(), ConfigError> {
        if self.targets.is_empty() {
            return Err(ConfigError::Validation(
                "targets.json must contain at least one target".into(),
            ));
        }
        if self.roms.is_empty() {
            return Err(ConfigError::Validation(
                "roms.json must contain at least one ROM".into(),
            ));
        }
        ensure_unique(
            self.targets.iter().map(|entry| entry.codename.as_str()),
            "target codename",
        )?;
        ensure_unique(
            self.roms.iter().map(|entry| entry.name.as_str()),
            "ROM name",
        )?;
        ensure_unique(
            self.recoveries.iter().map(|entry| entry.name.as_str()),
            "recovery name",
        )?;
        ensure_unique(
            self.manifests
                .iter()
                .map(|entry| entry.name.as_str())
                .chain(
                    self.recovery_manifests
                        .iter()
                        .map(|entry| entry.name.as_str()),
                ),
            "configuration name",
        )?;

        for manifest in &self.manifests {
            for branch in &manifest.branches {
                let rom = unique_named(&self.roms, &branch.target_rom, |entry| &entry.name)
                    .ok_or_else(|| {
                        ConfigError::Validation(format!(
                            "manifest '{}' references unknown or duplicate ROM '{}'",
                            manifest.name, branch.target_rom
                        ))
                    })?;
                if !rom
                    .branches
                    .iter()
                    .any(|entry| entry.android_version == branch.android_version)
                {
                    return Err(ConfigError::Validation(format!(
                        "manifest '{}' references Android {} for ROM '{}', but no matching ROM branch exists",
                        manifest.name, branch.android_version, branch.target_rom
                    )));
                }
                if !branch.use_regex
                    && !self
                        .targets
                        .iter()
                        .any(|entry| entry.codename == branch.device)
                {
                    return Err(ConfigError::Validation(format!(
                        "manifest '{}' references unknown target '{}'",
                        manifest.name, branch.device
                    )));
                }
                if branch.use_regex {
                    regex::Regex::new(&branch.device).map_err(|error| {
                        ConfigError::Validation(format!(
                            "manifest '{}' has invalid device regex '{}': {error}",
                            manifest.name, branch.device
                        ))
                    })?;
                }
            }
        }

        for manifest in &self.recovery_manifests {
            let recovery = unique_named(&self.recoveries, &manifest.target_recovery, |entry| {
                &entry.name
            })
            .ok_or_else(|| {
                ConfigError::Validation(format!(
                    "recovery manifest '{}' references unknown or duplicate recovery '{}'",
                    manifest.name, manifest.target_recovery
                ))
            })?;
            if !recovery
                .branches
                .iter()
                .any(|entry| entry.android_version == manifest.android_version)
            {
                return Err(ConfigError::Validation(format!(
                    "recovery manifest '{}' references Android {}, but recovery '{}' has no matching branch",
                    manifest.name, manifest.android_version, manifest.target_recovery
                )));
            }
            if !manifest.use_regex
                && !self
                    .targets
                    .iter()
                    .any(|entry| entry.codename == manifest.device)
            {
                return Err(ConfigError::Validation(format!(
                    "recovery manifest '{}' references unknown target '{}'",
                    manifest.name, manifest.device
                )));
            }
            if manifest.use_regex {
                regex::Regex::new(&manifest.device).map_err(|error| {
                    ConfigError::Validation(format!(
                        "recovery manifest '{}' has invalid device regex '{}': {error}",
                        manifest.name, manifest.device
                    ))
                })?;
            }
        }
        Ok(())
    }
}

fn load_json<T: DeserializeOwned>(path: &PathBuf) -> Result<T, ConfigError> {
    let file = std::fs::File::open(path).map_err(|error| ConfigError::Io {
        path: path.clone(),
        message: error.to_string(),
    })?;
    serde_json::from_reader(std::io::BufReader::new(file)).map_err(|error| ConfigError::Parse {
        path: path.clone(),
        message: error.to_string(),
    })
}

fn load_json_directory<T: DeserializeOwned>(directory: &PathBuf) -> Result<Vec<T>, ConfigError> {
    let entries = std::fs::read_dir(directory).map_err(|error| ConfigError::Io {
        path: directory.clone(),
        message: error.to_string(),
    })?;
    let mut paths = entries
        .map(|entry| {
            entry
                .map(|entry| entry.path())
                .map_err(|error| ConfigError::Io {
                    path: directory.clone(),
                    message: error.to_string(),
                })
        })
        .collect::<Result<Vec<_>, _>>()?;
    paths.retain(|path| path.extension().and_then(|value| value.to_str()) == Some("json"));
    paths.sort();
    paths.iter().map(load_json).collect()
}

fn ensure_unique<'a>(
    values: impl Iterator<Item = &'a str>,
    label: &str,
) -> Result<(), ConfigError> {
    let mut seen = std::collections::HashSet::new();
    for value in values {
        if !seen.insert(value) {
            return Err(ConfigError::Validation(format!(
                "duplicate {label}: '{value}'"
            )));
        }
    }
    Ok(())
}

fn unique_named<'a, T>(
    entries: &'a [T],
    name: &str,
    get_name: impl Fn(&T) -> &String,
) -> Option<&'a T> {
    let mut matches = entries
        .iter()
        .filter(|entry| get_name(entry).as_str() == name);
    let first = matches.next()?;
    if matches.next().is_some() {
        None
    } else {
        Some(first)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn loads_and_validates_repository_configuration() {
        let directory = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
            .parent()
            .expect("builder-rs has a parent directory")
            .join("android")
            .join("configs");
        let config = ROMBuildConfig::load(&directory)
            .unwrap_or_else(|error| panic!("repository config must be valid: {error}"));
        assert!(!config.targets.is_empty());
        assert!(!config.roms.is_empty());
    }

    #[test]
    fn missing_required_files_are_fatal() {
        let directory = tempfile::tempdir().expect("temporary directory");
        let error = ROMBuildConfig::load(&directory.path().to_path_buf())
            .expect_err("missing targets.json must fail");
        assert!(matches!(error, ConfigError::Io { .. }));
    }
}
