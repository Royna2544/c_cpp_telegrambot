use std::sync::Arc;

use tokio::sync::Mutex;

use super::service::{ActiveBuild, BuildEntry, UploadTask};

pub(crate) struct RomBuildRegistry {
    pub(crate) active: Arc<Mutex<Option<ActiveBuild>>>,
    pub(crate) known: Arc<Mutex<Vec<BuildEntry>>>,
    pub(crate) uploads: Arc<Mutex<Vec<UploadTask>>>,
}

impl RomBuildRegistry {
    pub(crate) fn new() -> Self {
        Self {
            active: Arc::new(Mutex::new(None)),
            known: Arc::new(Mutex::new(Vec::new())),
            uploads: Arc::new(Mutex::new(Vec::new())),
        }
    }

    #[cfg(test)]
    pub(crate) fn with_known(known: Arc<Mutex<Vec<BuildEntry>>>) -> Self {
        Self {
            active: Arc::new(Mutex::new(None)),
            known,
            uploads: Arc::new(Mutex::new(Vec::new())),
        }
    }
}
