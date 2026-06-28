use tracing::info;

use crate::build_common::BuildError;

use super::types::{
    ManifestBranchesEntry, ManifestEntry, ROMBranchEntry, ROMBuildConfig, ROMEntry,
    RecoveryManifestEntry, TargetsEntry,
};

pub enum ConfigType {
    Standard(ManifestEntry),
    Recovery(RecoveryManifestEntry),
}

pub struct ResolveRomConfigRequest<'a> {
    pub config_name: &'a str,
    pub target_device: &'a str,
    pub rom_name: &'a str,
    pub rom_android_version: f32,
}

pub struct ResolvedRomConfig {
    pub config_entry: ConfigType,
    pub device_entry: TargetsEntry,
    pub branch_entry: ManifestBranchesEntry,
    pub rom_entry: ROMEntry,
    pub rom_branch_entry: ROMBranchEntry,
}

pub struct RomConfigResolver<'a> {
    configs: &'a ROMBuildConfig,
}

impl<'a> RomConfigResolver<'a> {
    pub fn new(configs: &'a ROMBuildConfig) -> Self {
        Self { configs }
    }

    pub fn resolve(
        &self,
        request: ResolveRomConfigRequest<'_>,
    ) -> Result<ResolvedRomConfig, BuildError> {
        let config_entries = self
            .configs
            .manifests
            .iter()
            .filter(|entry| entry.name == request.config_name)
            .collect::<Vec<_>>();
        let recovery_entries = self
            .configs
            .recovery_manifests
            .iter()
            .filter(|entry| entry.name == request.config_name)
            .collect::<Vec<_>>();

        if config_entries.len() != 1 && recovery_entries.len() != 1 {
            return Err(BuildError::ConfigNotFound(format!(
                "No unique configuration found with name: {} (Got {} entries)",
                request.config_name,
                config_entries.len()
            )));
        }

        let config_entry = if config_entries.len() == 1 {
            ConfigType::Standard(config_entries[0].clone())
        } else {
            ConfigType::Recovery(recovery_entries[0].clone())
        };

        let device_entries = self
            .configs
            .targets
            .iter()
            .filter(|entry| entry.codename == request.target_device)
            .collect::<Vec<_>>();
        if device_entries.len() != 1 {
            return Err(BuildError::InvalidRequest(format!(
                "No unique device found with codename: {} (Got {} entries)",
                request.target_device,
                device_entries.len()
            )));
        }
        let device_entry = device_entries[0].clone();
        info!("Found device entry: {}", device_entry.codename);

        let (branch_entry, rom_entry, rom_branch_entry) = match &config_entry {
            ConfigType::Standard(config) => {
                let branches = config
                    .branches
                    .iter()
                    .filter(|entry| {
                        entry.target_rom == request.rom_name
                            && entry.android_version == request.rom_android_version
                            && (entry.device == request.target_device
                                || (entry.use_regex
                                    && regex::Regex::new(&entry.device)
                                        .map(|regex| regex.is_match(request.target_device))
                                        .unwrap_or(false)))
                    })
                    .collect::<Vec<_>>();
                if branches.len() != 1 {
                    for branch in &branches {
                        info!(
                            "Matching branch found: {} for device: {}",
                            branch.name, branch.device
                        );
                    }
                    return Err(BuildError::InvalidRequest(format!(
                        "No unique branch found for target device: {} (Got {} entries)",
                        request.target_device,
                        branches.len()
                    )));
                }
                let branch_entry = branches[0];
                info!(
                    "Found local manifest branch entry: {} for ROM: {}",
                    branch_entry.name, branch_entry.target_rom
                );

                let rom_entries = self
                    .configs
                    .roms
                    .iter()
                    .filter(|entry| entry.name == branch_entry.target_rom)
                    .collect::<Vec<_>>();
                if rom_entries.len() != 1 {
                    for rom in &rom_entries {
                        info!("Matching ROM found: {}", rom.name);
                    }
                    return Err(BuildError::ConfigNotFound(format!(
                        "No unique ROM found with name: {} (Got {} entries)",
                        branch_entry.target_rom,
                        rom_entries.len()
                    )));
                }
                let rom_entry = rom_entries[0];
                info!("Found ROM entry: {}", rom_entry.name);

                let rom_branches = rom_entry
                    .branches
                    .iter()
                    .filter(|entry| entry.android_version == branch_entry.android_version)
                    .collect::<Vec<_>>();
                if rom_branches.len() != 1 {
                    for rom_branch in &rom_branches {
                        info!("Matching ROM branch found: {}", rom_branch.branch);
                    }
                    return Err(BuildError::ConfigNotFound(format!(
                        "No unique ROM branch found with name: {} (Got {} entries)",
                        branch_entry.name,
                        rom_branches.len()
                    )));
                }
                let rom_branch_entry = rom_branches[0];
                info!("Found ROM branch entry: {}", rom_branch_entry.branch);

                (
                    branch_entry.clone(),
                    rom_entry.clone(),
                    rom_branch_entry.clone(),
                )
            }
            ConfigType::Recovery(config) => {
                let recoveries = self
                    .configs
                    .recoveries
                    .iter()
                    .filter(|entry| entry.name == config.target_recovery)
                    .collect::<Vec<_>>();
                if recoveries.len() != 1 {
                    return Err(BuildError::ConfigNotFound(format!(
                        "No unique recovery branch found with name: {} (Got {} entries)",
                        config.target_recovery,
                        recoveries.len()
                    )));
                }
                let recovery = recoveries[0];
                info!(
                    "Found recovery entry: {} for name: {}",
                    recovery.name, config.name
                );

                let branches = recovery
                    .branches
                    .iter()
                    .filter(|entry| entry.android_version == config.android_version)
                    .collect::<Vec<_>>();
                if branches.len() != 1 {
                    return Err(BuildError::ConfigNotFound(format!(
                        "No unique branch found for recovery: {} (Got {} entries)",
                        config.target_recovery,
                        branches.len()
                    )));
                }
                let rom_branch_entry = branches[0];
                let branch_entry = ManifestBranchesEntry {
                    name: String::new(),
                    target_rom: config.target_recovery.clone(),
                    android_version: rom_branch_entry.android_version,
                    device: config.device.clone(),
                    use_regex: config.use_regex,
                };

                (branch_entry, recovery.clone(), rom_branch_entry.clone())
            }
        };

        Ok(ResolvedRomConfig {
            config_entry,
            device_entry,
            branch_entry,
            rom_entry,
            rom_branch_entry,
        })
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::rombuild::types::{
        CloneMappingsEntry, ROMArtifactEntry, ROMArtifactMatcher, ROMBranchEntry,
    };

    fn artifact() -> ROMArtifactEntry {
        ROMArtifactEntry {
            matcher: ROMArtifactMatcher::ExactMatcher,
            data: "artifact.zip".into(),
        }
    }

    fn target() -> TargetsEntry {
        TargetsEntry {
            name: "Test Device".into(),
            codename: "device".into(),
            manufacturer: "Example".into(),
        }
    }

    #[test]
    fn resolves_standard_rom_config_graph() {
        let configs = ROMBuildConfig {
            roms: vec![ROMEntry {
                name: "lineage".into(),
                link: "https://example.com/manifest".into(),
                target: "bacon".into(),
                artifact: artifact(),
                branches: vec![ROMBranchEntry {
                    android_version: 15.0,
                    branch: "lineage-22.0".into(),
                }],
            }],
            recoveries: vec![],
            targets: vec![target()],
            manifests: vec![ManifestEntry {
                name: "default".into(),
                url: "https://example.com/local-manifest".into(),
                branches: vec![ManifestBranchesEntry {
                    name: "lineage-device".into(),
                    target_rom: "lineage".into(),
                    android_version: 15.0,
                    device: "device".into(),
                    use_regex: false,
                }],
            }],
            recovery_manifests: vec![],
        };

        let resolved = RomConfigResolver::new(&configs)
            .resolve(ResolveRomConfigRequest {
                config_name: "default",
                target_device: "device",
                rom_name: "lineage",
                rom_android_version: 15.0,
            })
            .expect("standard config should resolve");

        assert!(matches!(resolved.config_entry, ConfigType::Standard(_)));
        assert_eq!(resolved.device_entry.codename, "device");
        assert_eq!(resolved.branch_entry.name, "lineage-device");
        assert_eq!(resolved.rom_entry.name, "lineage");
        assert_eq!(resolved.rom_branch_entry.branch, "lineage-22.0");
    }

    #[test]
    fn resolves_recovery_config_graph() {
        let configs = ROMBuildConfig {
            roms: vec![],
            recoveries: vec![ROMEntry {
                name: "twrp".into(),
                link: "https://example.com/twrp".into(),
                target: "recoveryimage".into(),
                artifact: artifact(),
                branches: vec![ROMBranchEntry {
                    android_version: 14.0,
                    branch: "android-14".into(),
                }],
            }],
            targets: vec![target()],
            manifests: vec![],
            recovery_manifests: vec![RecoveryManifestEntry {
                name: "recovery".into(),
                clone_mappings: vec![CloneMappingsEntry {
                    repo: "https://example.com/device".into(),
                    branch: "android-14".into(),
                    path: "device/example/device".into(),
                }],
                android_version: 14.0,
                target_recovery: "twrp".into(),
                device: "device".into(),
                use_regex: false,
            }],
        };

        let resolved = RomConfigResolver::new(&configs)
            .resolve(ResolveRomConfigRequest {
                config_name: "recovery",
                target_device: "device",
                rom_name: "unused",
                rom_android_version: 0.0,
            })
            .expect("recovery config should resolve");

        assert!(matches!(resolved.config_entry, ConfigType::Recovery(_)));
        assert_eq!(resolved.device_entry.codename, "device");
        assert_eq!(resolved.branch_entry.target_rom, "twrp");
        assert_eq!(resolved.rom_entry.name, "twrp");
        assert_eq!(resolved.rom_branch_entry.branch, "android-14");
    }
}
