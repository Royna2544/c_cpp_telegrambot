use std::{fmt, path::PathBuf};

use crate::build_common::BuildId;

use super::config_resolver::ResolvedRomConfig;

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum RomBuildVariant {
    User,
    UserDebug,
    Eng,
}

impl fmt::Display for RomBuildVariant {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(match self {
            Self::User => "user",
            Self::UserDebug => "userdebug",
            Self::Eng => "eng",
        })
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum RomUploadMethod {
    None,
    LocalFile,
    Stream,
    GoFile,
    Unknown(i32),
}

#[derive(Clone, Debug)]
pub struct RomBuildRequest {
    pub config_name: String,
    pub target_codename: String,
    pub rom_name: String,
    pub rom_android_version: f32,
    pub variant: RomBuildVariant,
    pub force_checkout: bool,
    pub parallel_jobs: i32,
    pub github_token: Option<String>,
    pub upload_method: RomUploadMethod,
}

#[derive(Clone, Debug)]
pub struct RomBuildSettings {
    pub do_repo_sync: bool,
    pub do_clean_build: bool,
    pub use_ccache: bool,
    pub use_rbe_service: bool,
    pub rbe_api_token: Option<String>,
    pub do_upload: bool,
}

impl Default for RomBuildSettings {
    fn default() -> Self {
        Self {
            do_repo_sync: true,
            do_clean_build: false,
            use_ccache: false,
            use_rbe_service: false,
            rbe_api_token: None,
            do_upload: false,
        }
    }
}

#[derive(Clone, Debug)]
pub struct RomBuildDirs {
    pub build: PathBuf,
    pub temp: PathBuf,
}

pub struct RomBuildTask {
    pub id: BuildId,
    pub request: RomBuildRequest,
    pub settings: RomBuildSettings,
    pub dirs: RomBuildDirs,
    pub resolved: ResolvedRomConfig,
}
