use std::sync::Arc;

use tokio::sync::Mutex;

use super::service::{ActiveBuild, BuildEntry, UploadTask};

pub(crate) struct RomBuildRegistry {
    pub(crate) active: Arc<Mutex<Option<ActiveBuild>>>,
    pub(crate) known: Arc<Mutex<Vec<BuildEntry>>>,
    pub(crate) uploads: Arc<Mutex<Vec<UploadTask>>>,
}

impl RomBuildRegistry {
    pub(crate) const MAX_HISTORY: usize = 256;

    pub(crate) fn new() -> Self {
        Self {
            active: Arc::new(Mutex::new(None)),
            known: Arc::new(Mutex::new(Vec::new())),
            uploads: Arc::new(Mutex::new(Vec::new())),
        }
    }

    pub(crate) async fn push_known(&self, entry: BuildEntry) {
        let mut known = self.known.lock().await;
        known.push(entry);
        let excess = known.len().saturating_sub(Self::MAX_HISTORY);
        if excess > 0 {
            known.drain(..excess);
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

#[cfg(test)]
mod tests {
    use super::*;
    use crate::rombuild::{domain::RomBuildVariant, service::BuildStatus, types::TargetsEntry};

    fn entry(id: usize) -> BuildEntry {
        BuildEntry {
            id: id.to_string(),
            variant: RomBuildVariant::User,
            target_device: TargetsEntry {
                name: "n".into(),
                codename: "c".into(),
                manufacturer: "m".into(),
            },
            config_name: "cfg".into(),
            success: BuildStatus::Success,
        }
    }

    #[tokio::test]
    async fn known_build_registry_is_bounded() {
        let registry = RomBuildRegistry::new();
        for id in 0..300 {
            registry.push_known(entry(id)).await;
        }
        let known = registry.known.lock().await;
        assert_eq!(known.len(), RomBuildRegistry::MAX_HISTORY);
        assert_eq!(known[0].id, "44");
    }
}
