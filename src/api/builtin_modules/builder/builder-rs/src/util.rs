//! Utility functions for file handling and path manipulation.
//!
//! This module provides common utility functions used throughout the builder-rs service,
//! including JSON file processing, path canonicalization, and generic deserialization helpers.

use std::path::PathBuf;

use serde::Deserialize;
use tracing::{debug, error, warn};

/// Processes all JSON files in a directory with a provided function.
///
/// # Arguments
///
/// * `json_dir` - The directory containing JSON files
/// * `func` - A function that processes each JSON file path and returns a result
///
/// # Returns
///
/// A vector of successfully processed results
pub fn for_each_json_file<T>(
    json_dir: &PathBuf,
    func: impl Fn(&PathBuf) -> Result<T, ()>,
) -> Vec<T> {
    let entries: std::fs::ReadDir;
    let mut results: Vec<T> = Vec::new();
    match std::fs::read_dir(&json_dir) {
        Err(e) => {
            error!(
                "Failed to read JSON directory {}: {}",
                json_dir.display(),
                e.to_string()
            );
            return results;
        }
        Ok(value) => entries = value,
    }
    for entry in entries {
        let entry = match entry {
            Err(e) => {
                error!("Failed to read directory entry: {}", e);
                continue;
            }
            Ok(val) => val,
        };
        let path = entry.path();
        if path.extension().and_then(|s| s.to_str()) == Some("json") {
            debug!("Found JSON file: {:?}", path);
            let t = func(&path);
            match t {
                Ok(val) => results.push(val),
                Err(_) => {
                    continue;
                }
            }
        }
    }
    results
}

/// Canonicalizes a path, returning None if the path doesn't exist or can't be resolved.
///
/// # Arguments
///
/// * `path` - The path to canonicalize
///
/// # Returns
///
/// The canonical path or None if canonicalization fails
pub fn make_canonical_path(path: &PathBuf) -> Option<PathBuf> {
    match std::fs::canonicalize(path) {
        Ok(canonical) => Some(canonical),
        Err(e) => {
            error!("Failed to canonicalize path {}: {}", path.display(), e);
            None
        }
    }
}

/// Canonicalizes a path, creating parent directories if they don't exist.
///
/// # Arguments
///
/// * `path` - The path to canonicalize and potentially create
///
/// # Returns
///
/// The canonical path or None if creation or canonicalization fails
pub fn make_canonical_path_mkdirs(path: &PathBuf) -> Option<PathBuf> {
    match std::fs::canonicalize(path) {
        Ok(canonical) => Some(canonical),
        Err(_e) => {
            warn!(
                "Path {} does not exist. Attempting to create it.",
                path.display()
            );
            match std::fs::create_dir_all(path) {
                Ok(_) => match std::fs::canonicalize(path) {
                    Ok(canonical) => Some(canonical),
                    Err(e) => {
                        error!(
                            "Failed to canonicalize path {} after creation: {}",
                            path.display(),
                            e
                        );
                        None
                    }
                },
                Err(e) => {
                    error!("Failed to create directory {}: {}", path.display(), e);
                    None
                }
            }
        }
    }
}

/// Generic helper to deserialize a JSON file into a type T.
///
/// # Type Parameters
///
/// * `T` - The type to deserialize into, must implement Deserialize
///
/// # Arguments
///
/// * `file_path` - Path to the JSON file
///
/// # Returns
///
/// The deserialized object or an error
pub fn new_impl<T>(file_path: &PathBuf) -> Result<T, ()>
where
    T: for<'de> Deserialize<'de>,
{
    let file = std::fs::File::open(file_path).or_else(|err| {
        error!("Failed to open file {:?}: {}", file_path, err);
        Err(())
    })?;
    let reader = std::io::BufReader::new(file);
    let config: T = serde_json::from_reader(reader).map_err(|x| {
        error!("JSON parsing error: {}", x);
        ()
    })?;
    Ok(config)
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;
    use std::io::Write;
    use tempfile::TempDir;

    #[test]
    fn test_make_canonical_path_existing() {
        let temp_dir = TempDir::new().unwrap();
        let path = temp_dir.path().to_path_buf();

        let canonical = make_canonical_path(&path);
        assert!(canonical.is_some());
        assert!(canonical.unwrap().exists());
    }

    #[test]
    fn test_make_canonical_path_nonexistent() {
        let path = PathBuf::from("/nonexistent/path/that/does/not/exist");
        let canonical = make_canonical_path(&path);
        assert!(canonical.is_none());
    }

    #[test]
    fn test_make_canonical_path_mkdirs_creates_directory() {
        let temp_dir = TempDir::new().unwrap();
        let new_dir = temp_dir.path().join("new").join("nested").join("dir");

        let canonical = make_canonical_path_mkdirs(&new_dir);
        assert!(canonical.is_some());
        assert!(canonical.unwrap().exists());
    }

    #[test]
    fn test_make_canonical_path_mkdirs_existing() {
        let temp_dir = TempDir::new().unwrap();
        let path = temp_dir.path().to_path_buf();

        let canonical = make_canonical_path_mkdirs(&path);
        assert!(canonical.is_some());
        assert!(canonical.unwrap().exists());
    }

    #[test]
    fn test_for_each_json_file_empty_dir() {
        let temp_dir = TempDir::new().unwrap();

        let results: Vec<String> = for_each_json_file(&temp_dir.path().to_path_buf(), |_path| {
            Ok("test".to_string())
        });

        assert_eq!(results.len(), 0);
    }

    #[test]
    fn test_for_each_json_file_with_json_files() {
        let temp_dir = TempDir::new().unwrap();

        // Create test JSON files
        let json1 = temp_dir.path().join("test1.json");
        let json2 = temp_dir.path().join("test2.json");
        let non_json = temp_dir.path().join("test.txt");

        fs::write(&json1, r#"{"key": "value1"}"#).unwrap();
        fs::write(&json2, r#"{"key": "value2"}"#).unwrap();
        fs::write(&non_json, "not json").unwrap();

        let results: Vec<String> = for_each_json_file(&temp_dir.path().to_path_buf(), |path| {
            Ok(path.file_name().unwrap().to_string_lossy().to_string())
        });

        assert_eq!(results.len(), 2);
        assert!(results.contains(&"test1.json".to_string()));
        assert!(results.contains(&"test2.json".to_string()));
        assert!(!results.contains(&"test.txt".to_string()));
    }

    #[test]
    fn test_new_impl_valid_json() {
        #[derive(Deserialize)]
        struct TestConfig {
            key: String,
        }

        let temp_dir = TempDir::new().unwrap();
        let json_path = temp_dir.path().join("config.json");

        let mut file = fs::File::create(&json_path).unwrap();
        file.write_all(br#"{"key": "value"}"#).unwrap();

        let config: Result<TestConfig, ()> = new_impl(&json_path);
        assert!(config.is_ok());
        assert_eq!(config.unwrap().key, "value");
    }

    #[test]
    fn test_new_impl_invalid_json() {
        #[derive(Deserialize)]
        struct TestConfig {
            key: String,
        }

        let temp_dir = TempDir::new().unwrap();
        let json_path = temp_dir.path().join("bad.json");

        let mut file = fs::File::create(&json_path).unwrap();
        file.write_all(b"not valid json").unwrap();

        let config: Result<TestConfig, ()> = new_impl(&json_path);
        assert!(config.is_err());
    }

    #[test]
    fn test_new_impl_nonexistent_file() {
        #[derive(Deserialize)]
        struct TestConfig {
            key: String,
        }

        let path = PathBuf::from("/nonexistent/config.json");
        let config: Result<TestConfig, ()> = new_impl(&path);
        assert!(config.is_err());
    }
}
