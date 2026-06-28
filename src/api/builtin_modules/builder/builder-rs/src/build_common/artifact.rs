use std::path::PathBuf;

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BuildArtifact {
    pub path: PathBuf,
    pub file_name: String,
    pub kind: ArtifactKind,
}

impl BuildArtifact {
    pub fn new(path: PathBuf, kind: ArtifactKind) -> Result<Self, String> {
        let file_name = path
            .file_name()
            .ok_or_else(|| format!("Artifact path has no file name: {}", path.display()))?
            .to_string_lossy()
            .into_owned();
        Ok(Self {
            path,
            file_name,
            kind,
        })
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum ArtifactKind {
    RomZip,
    RecoveryImage,
    KernelImage,
    KernelZip,
    LogArchive,
}
