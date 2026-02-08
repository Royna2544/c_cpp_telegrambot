//! GoFile Upload CLI Tool
//!
//! A command-line utility for uploading files to GoFile.io.
//! This tool provides a simple interface for uploading build artifacts
//! and other files to the GoFile hosting service.
//!
//! # Usage
//!
//! ```bash
//! gofile-upload <FILE_PATH>
//! ```
//!
//! # Examples
//!
//! Upload a single file:
//! ```bash
//! gofile-upload /path/to/artifact.zip
//! ```

use clap::Parser;
use std::path::Path;
use tracing::{error, info};
use tracing_subscriber::EnvFilter;

// Import the gofile_api module from the builder crate
use builder::gofile_api::{UploadFileResponse, upload_file_to_gofile};

#[derive(Parser, Debug)]
#[command(
    name = "gofile-upload",
    version,
    about = "Upload files to GoFile.io",
    long_about = "A command-line utility for uploading files to the GoFile.io hosting service.\n\
                  This tool uploads files and returns a download link that can be shared."
)]
struct Args {
    /// Path to the file to upload
    #[arg(value_name = "FILE")]
    file_path: String,

    /// Enable verbose logging
    #[arg(short, long)]
    verbose: bool,
}

fn print_upload_result(response: &UploadFileResponse) {
    println!("\n✓ Upload successful!");
    println!("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    println!("File Name:     {}", response.data.name);
    println!("File Size:     {} bytes", response.data.size);
    println!("File ID:       {}", response.data.id);
    println!("MD5 Checksum:  {}", response.data.md5);
    println!("MIME Type:     {}", response.data.mimetype);
    println!("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    println!("📥 Download Page: {}", response.data.downloadPage);
    println!("━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━");
    println!("\nShare this link to allow others to download your file.");
}

#[tokio::main]
async fn main() {
    let args = Args::parse();

    // Initialize logging
    let filter = if args.verbose {
        EnvFilter::new("debug")
    } else {
        EnvFilter::new("info")
    };

    tracing_subscriber::fmt()
        .with_env_filter(filter)
        .with_file(true)
        .with_line_number(true)
        .with_target(false)
        .init();

    // Validate file exists
    let file_path = Path::new(&args.file_path);
    if !file_path.exists() {
        error!("File does not exist: {}", args.file_path);
        eprintln!("❌ Error: File '{}' does not exist", args.file_path);
        std::process::exit(1);
    }

    if !file_path.is_file() {
        error!("Path is not a file: {}", args.file_path);
        eprintln!("❌ Error: '{}' is not a file", args.file_path);
        std::process::exit(1);
    }

    // Get file size for display
    let file_size = match std::fs::metadata(file_path) {
        Ok(metadata) => metadata.len(),
        Err(e) => {
            error!("Failed to read file metadata: {}", e);
            eprintln!("❌ Error: Failed to read file metadata: {}", e);
            std::process::exit(1);
        }
    };

    info!("Uploading file: {}", args.file_path);
    info!(
        "File size: {} bytes ({:.2} MB)",
        file_size,
        file_size as f64 / 1_048_576.0
    );

    println!("📤 Uploading file: {}", args.file_path);
    println!(
        "📊 File size: {:.2} MB ({} bytes)",
        file_size as f64 / 1_048_576.0,
        file_size
    );
    println!("⏳ Please wait...\n");

    // Upload the file
    match upload_file_to_gofile(&args.file_path).await {
        Ok(response) => {
            info!(
                "Upload successful! Download page: {}",
                response.data.downloadPage
            );
            print_upload_result(&response);
            std::process::exit(0);
        }
        Err(e) => {
            error!("Upload failed: {}", e);
            eprintln!("\n❌ Upload failed: {}", e);
            eprintln!("\nPossible causes:");
            eprintln!("  • Network connection issues");
            eprintln!("  • GoFile.io service unavailable");
            eprintln!("  • File too large for free tier");
            eprintln!("  • Rate limiting");
            eprintln!("\nTry again later or check your network connection.");
            std::process::exit(1);
        }
    }
}
