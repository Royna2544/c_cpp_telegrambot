use std::fmt::{self, Display};

#[derive(Clone, Debug, Eq, Hash, PartialEq)]
pub struct BuildId(String);

impl BuildId {
    pub fn new(value: impl Into<String>) -> Self {
        Self(value.into())
    }

    pub fn as_str(&self) -> &str {
        &self.0
    }

    pub fn into_string(self) -> String {
        self.0
    }
}

impl Display for BuildId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.write_str(&self.0)
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum BuildTerminalState {
    Success,
    Failed,
    Cancelled,
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BuildOutcome {
    pub state: BuildTerminalState,
    pub message: Option<String>,
}
