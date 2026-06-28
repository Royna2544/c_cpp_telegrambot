use std::{fmt, io};

use super::BuildId;

#[derive(Debug)]
pub enum BuildError {
    InvalidRequest(String),
    ConfigNotFound(String),
    AlreadyRunning,
    NotRunning,
    BuildNotFound(BuildId),
    Cancelled,
    MissingTool(String),
    CommandFailed {
        step: String,
        program: String,
        exit_code: Option<i32>,
    },
    ArtifactNotFound,
    UploadFailed(String),
    Io(String),
    Internal(String),
}

impl BuildError {
    pub fn internal(message: impl Into<String>) -> Self {
        Self::Internal(message.into())
    }
}

impl fmt::Display for BuildError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            Self::InvalidRequest(message)
            | Self::ConfigNotFound(message)
            | Self::UploadFailed(message)
            | Self::Io(message)
            | Self::Internal(message) => f.write_str(message),
            Self::AlreadyRunning => f.write_str("A build is already running"),
            Self::NotRunning => f.write_str("No build is currently running"),
            Self::BuildNotFound(id) => write!(f, "Build not found: {id}"),
            Self::Cancelled => f.write_str("Build cancelled"),
            Self::MissingTool(tool) => write!(f, "Required tool is unavailable: {tool}"),
            Self::CommandFailed {
                step,
                program,
                exit_code,
            } => write!(
                f,
                "Command failed during {step}: {program} (exit code: {exit_code:?})"
            ),
            Self::ArtifactNotFound => f.write_str("Build artifact not found"),
        }
    }
}

impl std::error::Error for BuildError {}

impl From<io::Error> for BuildError {
    fn from(error: io::Error) -> Self {
        Self::Io(error.to_string())
    }
}
