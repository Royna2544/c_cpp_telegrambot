use tracing::{error, info};

#[derive(serde::Deserialize, Debug)]
pub struct ServerEntry {
    pub name: String,
    pub zone: String,
}

#[derive(serde::Deserialize, Debug)]
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
pub struct UploadFileResponseData {
    pub createTime: u32,          // Unix timestamp
    pub downloadPage: String,     // URL to download page
    pub guestToken: String,       // Guest token for file management
    pub id: String,               // File ID
    pub md5: String,              // MD5 checksum of the file
    pub mimeType: String,         // MIME type of the file
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
        let resp = reqwest::get("https://api.gofile.io/servers")
            .await
            .inspect_err(|x| {
                error!("Cannot GET /servers: {}", x);
            })?;
        let resp: ServersJson = serde_json::from_str(&resp.text().await.inspect_err(|x| {
            error!("Cannot obtain servers list in str: {}", x);
        })?)
        .inspect_err(|x| error!("Cannot parse JSON from response: {}", x))?;

        if resp.status != "ok" {
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
        })?;

    if upload_response.status != "ok" {
        Err(format!(
            "Gofile upload returned non-ok status: {}",
            upload_response.status
        ))?
    }

    Ok(upload_response)
}
