use chrono::{DateTime, Utc};

use super::{BuildArtifact, BuildId, BuildOutcome};

#[derive(Clone, Debug)]
pub enum BuildEvent {
    Started {
        build_id: BuildId,
        at: DateTime<Utc>,
    },
    StepStarted {
        name: String,
        at: DateTime<Utc>,
    },
    StepFinished {
        name: String,
        at: DateTime<Utc>,
    },
    Stdout {
        line: String,
        first: bool,
        at: DateTime<Utc>,
    },
    Stderr {
        line: String,
        first: bool,
        at: DateTime<Utc>,
    },
    Progress {
        message: String,
        percent: Option<u8>,
        at: DateTime<Utc>,
    },
    Warning {
        message: String,
        at: DateTime<Utc>,
    },
    Error {
        message: String,
        at: DateTime<Utc>,
    },
    ArtifactFound {
        artifact: BuildArtifact,
        at: DateTime<Utc>,
    },
    UploadStarted {
        artifact: BuildArtifact,
        at: DateTime<Utc>,
    },
    UploadFinished {
        artifact: BuildArtifact,
        url: String,
        at: DateTime<Utc>,
    },
    Finished {
        outcome: BuildOutcome,
        at: DateTime<Utc>,
    },
}
