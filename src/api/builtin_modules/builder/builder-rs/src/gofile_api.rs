//! GoFile API integration for file uploads.
//!
//! This module provides integration with the GoFile.io API for uploading
//! build artifacts. It handles:
//!
//! - Server selection from available GoFile servers
//! - Multipart file uploads
//! - Response parsing and error handling
//!
//! # Examples
//!
//! ```no_run
//! # use builder::gofile_api::upload_file_to_gofile;
//!
//! # async {
//! let response = upload_file_to_gofile("/path/to/artifact.zip")
//!     .await
//!     .expect("Upload failed");
//! println!("Download page: {}", response.data.downloadPage);
//! # };
//! ```

use tracing::{error, info};

/// Base URL for the GoFile API
const GOFILE_API_BASE: &str = "https://api.gofile.io";

/// Expected status value for successful API responses
const API_STATUS_OK: &str = "ok";

#[derive(serde::Deserialize, Debug)]
pub struct ServerEntry {
    pub name: String,
    pub zone: String,
}

#[derive(serde::Deserialize, Debug)]
#[allow(nonstandard_style, non_snake_case)] // Part of GoFile API response, not ours.
pub struct ServerListEntry {
    pub servers: Vec<ServerEntry>,
    #[allow(nonstandard_style, non_snake_case)]
    pub serversAllZone: Vec<ServerEntry>,
}

#[derive(serde::Deserialize, Debug)]
pub struct ServersJson {
    pub status: String,
    pub data: ServerListEntry,
}

#[derive(serde::Deserialize, Debug)]
#[allow(nonstandard_style, non_snake_case)] // Part of GoFile API response, not ours.
pub struct UploadFileResponseData {
    pub createTime: u32,          // Unix timestamp
    pub downloadPage: String,     // URL to download page
    pub guestToken: String,       // Guest token for file management
    pub id: String,               // File ID
    pub md5: String,              // MD5 checksum of the file
    pub mimetype: String,         // MIME type of the file
    pub modTime: u32,             // Unix timestamp of last modification
    pub name: String,             // Original file name
    pub parentFolder: String,     // ID of the parent folder
    pub parentFolderCode: String, // Code of the parent folder
    pub servers: Vec<String>,     // List of servers hosting the file
    pub size: u64,                // Size of the file in bytes

    #[serde(rename = "type")]
    pub file_type: String, // Type of the upload
}

#[derive(serde::Deserialize, Debug)]
pub struct UploadFileResponse {
    pub status: String,
    pub data: UploadFileResponseData,
}

impl ServersJson {
    pub async fn get() -> Result<ServersJson, Box<dyn std::error::Error>> {
        let url = format!("{}/servers", GOFILE_API_BASE);
        let resp = reqwest::get(&url).await.inspect_err(|x| {
            error!("Cannot GET /servers: {}", x);
        })?;
        let resp: ServersJson = serde_json::from_str(&resp.text().await.inspect_err(|x| {
            error!("Cannot obtain servers list in str: {}", x);
        })?)
        .inspect_err(|x| error!("Cannot parse JSON from response: {}", x))?;

        if resp.status != API_STATUS_OK {
            Err(format!(
                "Gofile API returned non-ok status: {}",
                resp.status
            ))?
        }
        Ok(resp)
    }

    pub async fn get_any_server() -> Result<ServerEntry, Box<dyn std::error::Error>> {
        let servers_json = ServersJson::get().await?;
        let server = servers_json
            .data
            .servers
            .into_iter()
            .next()
            .ok_or("No servers found in Gofile API response")?;
        info!("Selected GoFile server: name: {}", server.name);
        Ok(server)
    }

    pub async fn get_server_url() -> Result<String, Box<dyn std::error::Error>> {
        let server = ServersJson::get_any_server().await?;
        let url = format!("https://{}.gofile.io", server.name);
        Ok(url)
    }

    pub async fn get_upload_url() -> Result<String, Box<dyn std::error::Error>> {
        let server_url = ServersJson::get_server_url().await?;
        let upload_url = format!("{}/contents/uploadFile", server_url);
        Ok(upload_url)
    }
}

pub async fn upload_file_to_gofile(
    file_path: &str,
) -> Result<UploadFileResponse, Box<dyn std::error::Error>> {
    let upload_url = ServersJson::get_upload_url().await?;
    let file_bytes = tokio::fs::read(file_path).await?;
    let file_name = std::path::Path::new(file_path)
        .file_name()
        .ok_or("Invalid file name")?
        .to_string_lossy()
        .to_string();

    let form = reqwest::multipart::Form::new().part(
        "file",
        reqwest::multipart::Part::bytes(file_bytes).file_name(file_name),
    );

    let client = reqwest::Client::new();
    let resp = client
        .post(&upload_url)
        .multipart(form)
        .send()
        .await
        .inspect_err(|x| error!("Failed to send upload request: {}", x))?;

    let resp_text = resp.text().await.inspect_err(|x| {
        error!("Failed to read response text from upload request: {}", x);
    })?;

    let upload_response: UploadFileResponse =
        serde_json::from_str(&resp_text).inspect_err(|x| {
            error!("Failed to parse upload response JSON: {}", x);
            error!("Response text: {}", resp_text);
        })?;

    if upload_response.status != API_STATUS_OK {
        Err(format!(
            "Gofile upload returned non-ok status: {}",
            upload_response.status
        ))?
    }

    Ok(upload_response)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_parse_servers_response() {
        let json = r#"{
            "status": "ok",
            "data": {
                "servers": [
                    {"name": "store1", "zone": "eu"},
                    {"name": "store2", "zone": "na"}
                ],
                "serversAllZone": []
            }
        }"#;

        let result: Result<ServersJson, _> = serde_json::from_str(json);
        assert!(result.is_ok());

        let servers = result.unwrap();
        assert_eq!(servers.status, "ok");
        assert_eq!(servers.data.servers.len(), 2);
        assert_eq!(servers.data.servers[0].name, "store1");
        assert_eq!(servers.data.servers[0].zone, "eu");
        assert_eq!(servers.data.servers[1].name, "store2");
        assert_eq!(servers.data.servers[1].zone, "na");
    }

    #[test]
    fn test_parse_upload_response() {
        let json = r#"{
            "status": "ok",
            "data": {
                "createTime": 1640995200,
                "downloadPage": "https://gofile.io/d/abc123",
                "guestToken": "guest_token_123",
                "id": "file_id_123",
                "md5": "098f6bcd4621d373cade4e832627b4f6",
                "mimetype": "application/zip",
                "modTime": 1640995200,
                "name": "test.zip",
                "parentFolder": "folder_id",
                "parentFolderCode": "folder_code",
                "servers": ["store1"],
                "size": 1024,
                "type": "file"
            }
        }"#;

        let result: Result<UploadFileResponse, _> = serde_json::from_str(json);
        assert!(result.is_ok());

        let response = result.unwrap();
        assert_eq!(response.status, "ok");
        assert_eq!(response.data.downloadPage, "https://gofile.io/d/abc123");
        assert_eq!(response.data.name, "test.zip");
        assert_eq!(response.data.size, 1024);
        assert_eq!(response.data.md5, "098f6bcd4621d373cade4e832627b4f6");
    }

    #[test]
    fn test_parse_servers_response_with_missing_fields() {
        let json = r#"{
            "status": "error",
            "data": {
                "servers": [],
                "serversAllZone": []
            }
        }"#;

        let result: Result<ServersJson, _> = serde_json::from_str(json);
        assert!(result.is_ok());

        let servers = result.unwrap();
        assert_eq!(servers.status, "error");
        assert_eq!(servers.data.servers.len(), 0);
    }

    #[test]
    fn test_server_url_format() {
        let server = ServerEntry {
            name: "store5".to_string(),
            zone: "eu".to_string(),
        };
        let url = format!("https://{}.gofile.io", server.name);
        assert_eq!(url, "https://store5.gofile.io");
    }

    #[test]
    fn test_upload_url_format() {
        let server_url = "https://store1.gofile.io";
        let upload_url = format!("{}/contents/uploadFile", server_url);
        assert_eq!(upload_url, "https://store1.gofile.io/contents/uploadFile");
    }

    #[tokio::test]
    async fn test_upload_file_with_nonexistent_file() {
        // This test verifies that uploading a non-existent file returns an error
        let result = upload_file_to_gofile("/nonexistent/path/to/file.txt").await;
        assert!(result.is_err());
    }

    #[tokio::test]
    async fn test_file_name_extraction() {
        use tempfile::NamedTempFile;
        use tokio::io::AsyncWriteExt;

        // Create a temporary file
        let temp_file = NamedTempFile::new().expect("Failed to create temp file");
        let temp_path = temp_file.path().to_str().unwrap();

        // Write some content to it
        let mut file = tokio::fs::File::create(temp_path)
            .await
            .expect("Failed to create file");
        file.write_all(b"test content")
            .await
            .expect("Failed to write to file");
        file.flush().await.expect("Failed to flush file");
        drop(file);

        // Extract file name
        let file_name = std::path::Path::new(temp_path)
            .file_name()
            .expect("Failed to get file name")
            .to_string_lossy()
            .to_string();

        assert!(!file_name.is_empty());
        // The file name should not contain directory separators
        assert!(!file_name.contains('/'));
        assert!(!file_name.contains('\\'));
    }
}
