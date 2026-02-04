use serde::{Deserialize, Serialize};

#[derive(Debug, Deserialize, Serialize, Clone)]
enum ROMArtifactMatcher {
    ZipFilePrefixer,
    ExactFileMatcher,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
struct ROMArtifactEntry {
    pub matcher: ROMArtifactMatcher,
    pub data: String,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
struct ROMBranchEntry {
    pub android_version: i32,
    pub branch: String,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
struct ROMEntry {
    pub name: String,
    pub link: String,
    pub target: String,
    pub artifact: ROMArtifactEntry,
    pub branches: Vec<ROMBranchEntry>,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
struct TargetsEntry {
    pub target: String,
    pub name: String,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
struct ManifestBranchesEntry {
    pub name: String,
    pub target_rom: String,
    pub android_version: i32,
    pub devices: Vec<String>,
}

#[derive(Debug, Deserialize, Serialize, Clone)]
struct ManifestEntry {
    name: String,
    url: String,
    branches: Vec<ManifestBranchesEntry>,
}
