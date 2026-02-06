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
