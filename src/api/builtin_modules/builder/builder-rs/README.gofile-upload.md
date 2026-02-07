# GoFile Upload Tool

A command-line utility for uploading files to [GoFile.io](https://gofile.io).

## Overview

The `gofile-upload` tool provides a simple interface for uploading build artifacts and other files to the GoFile hosting service. It automatically selects an available GoFile server, uploads your file, and returns a shareable download link.

## Features

- **Automatic Server Selection**: Automatically queries and selects an available GoFile server
- **Progress Information**: Displays file size and upload status
- **Error Handling**: Provides clear error messages and troubleshooting suggestions
- **Verbose Mode**: Optional detailed logging for debugging
- **File Validation**: Validates file existence and readability before upload

## Installation

Build the tool using Cargo:

```bash
cd src/api/builtin_modules/builder/builder-rs
cargo build --release --bin gofile-upload
```

The binary will be available at `target/release/gofile-upload`.

## Usage

### Basic Upload

Upload a single file:

```bash
gofile-upload /path/to/file.zip
```

### Verbose Mode

Enable detailed logging:

```bash
gofile-upload --verbose /path/to/file.zip
```

### Help

Display usage information:

```bash
gofile-upload --help
```

## Output

On successful upload, the tool displays:

- File name
- File size
- File ID
- MD5 checksum
- MIME type
- Download page URL

Example output:

```
✓ Upload successful!
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
File Name:     artifact.zip
File Size:     1048576 bytes
File ID:       abc123xyz
MD5 Checksum:  098f6bcd4621d373cade4e832627b4f6
MIME Type:     application/zip
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📥 Download Page: https://gofile.io/d/abc123
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Share this link to allow others to download your file.
```

## Error Handling

The tool provides helpful error messages for common issues:

- File not found
- Path is not a file
- Network connection issues
- GoFile service unavailable
- File too large for free tier
- Rate limiting

## API Integration

The tool uses the GoFile API which provides:

- Free file hosting
- No registration required
- Automatic server selection
- Shareable download links

For more information about GoFile API, see: https://gofile.io/

## Testing

Run the tests:

```bash
cargo test gofile
```

## Dependencies

- `reqwest`: HTTP client for API requests
- `tokio`: Async runtime
- `clap`: Command-line argument parsing
- `tracing`: Logging framework
- `serde`: JSON serialization/deserialization

## License

This tool is part of the c_cpp_telegrambot project. See the project's LICENSE file for details.
