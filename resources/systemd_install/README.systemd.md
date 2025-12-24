# Systemd Service Setup for TgBot++

This guide explains how to set up TgBot++ as a systemd service on Linux systems.

## Prerequisites

- TgBot++ compiled and installed on your system
- Configuration file set up (see Configuration section below)
- Root or sudo access to install system services

## Configuration

Before setting up the systemd service, ensure you have a configuration file in your home directory. The bot searches for configuration files in the following order:

1. `~/tgbotserver.debug.ini` (for debug builds)
2. `~/tgbotserver.release.ini` (for release builds)
3. `~/tgbotserver.ini` (fallback)

The configuration file should contain at minimum:

```ini
TOKEN=your_telegram_bot_token_here
```

For additional configuration options, see the main README.md file.

## Installation Steps

### 1. Prepare the Service File

Copy the template service file and customize it:

```bash
sudo cp resources/systemd_install/tgbot.service /etc/systemd/system/
sudo nano /etc/systemd/system/tgbot.service
```

### 2. Configure the Service File

Replace the following placeholders in `/etc/systemd/system/tgbot.service`:

- `<USER>`: The system user that will run the bot (e.g., `tgbot` or your username)
  ```ini
  User=tgbot
  ```

- `<WORKING_DIRECTORY>`: The directory where the bot should run from (typically the home directory of the user)
  ```ini
  WorkingDirectory=/home/tgbot
  ```

- `<EXECUTABLE_PATH>`: The full path to the TgBot++ executable
  ```ini
  ExecStart=/usr/local/bin/TgBot++_main
  ```
  or if using the daemon version:
  ```ini
  ExecStart=/usr/local/bin/TgBot++_maind
  ```

**Example of a complete service file:**

```ini
[Unit]
Description=TgBot++ Telegram Bot Service
After=network.target
Documentation=https://github.com/Royna2544/c_cpp_telegrambot

[Service]
Type=simple
User=tgbot
WorkingDirectory=/home/tgbot
ExecStart=/usr/local/bin/TgBot++_main
Restart=on-failure
RestartSec=10
StandardOutput=journal
StandardError=journal

# Security settings
NoNewPrivileges=true
PrivateTmp=true

[Install]
WantedBy=multi-user.target
```

### 3. Create a Dedicated User (Optional but Recommended)

For security reasons, it's recommended to run the bot under a dedicated user:

```bash
sudo useradd -r -m -d /home/tgbot -s /bin/bash tgbot
```

Then set up the configuration file for this user:

```bash
sudo -u tgbot nano /home/tgbot/tgbotserver.ini
```

### 4. Reload Systemd and Enable the Service

```bash
sudo systemctl daemon-reload
sudo systemctl enable tgbot.service
```

### 5. Start the Service

```bash
sudo systemctl start tgbot.service
```

## Managing the Service

### Check Service Status

```bash
sudo systemctl status tgbot.service
```

### View Logs

```bash
sudo journalctl -u tgbot.service -f
```

To view logs from the last boot:

```bash
sudo journalctl -u tgbot.service -b
```

### Stop the Service

```bash
sudo systemctl stop tgbot.service
```

### Restart the Service

```bash
sudo systemctl restart tgbot.service
```

### Disable the Service

```bash
sudo systemctl disable tgbot.service
```

## Troubleshooting

### Service Fails to Start

1. Check the service status and logs:
   ```bash
   sudo systemctl status tgbot.service
   sudo journalctl -u tgbot.service -n 50
   ```

2. Verify the configuration file exists and has correct permissions:
   ```bash
   ls -l /home/tgbot/tgbotserver.ini
   ```

3. Ensure the user has read access to the configuration file and execute permissions for the bot binary

4. Test running the bot manually as the service user:
   ```bash
   sudo -u tgbot /usr/local/bin/TgBot++_main
   ```

### Permission Issues

If the bot needs to access specific files or directories, ensure the service user has appropriate permissions:

```bash
sudo chown -R tgbot:tgbot /path/to/bot/files
```

### Configuration Not Found

Ensure the `WorkingDirectory` in the service file matches the directory containing your configuration file (typically the user's home directory).

## Security Considerations

The service file includes basic security hardening:

- `NoNewPrivileges=true`: Prevents privilege escalation
- `PrivateTmp=true`: Provides isolated temporary directory

For additional security, consider:

- Running the bot under a dedicated user with minimal permissions
- Using systemd's additional sandboxing options (ProtectSystem, ProtectHome, etc.)
- Keeping the configuration file readable only by the service user:
  ```bash
  chmod 600 /home/tgbot/tgbotserver.ini
  ```

## Alternative Configuration

If you prefer to use environment variables or command-line arguments instead of a configuration file, you can modify the `ExecStart` line:

### Using Environment Variables

Add an `EnvironmentFile` directive:

```ini
[Service]
EnvironmentFile=/etc/tgbot/environment
ExecStart=/usr/local/bin/TgBot++_main
```

And create `/etc/tgbot/environment`:
```bash
TOKEN=your_telegram_bot_token_here
LOG_FILE=/var/log/tgbot/bot.log
```

### Using Command-Line Arguments

Modify the `ExecStart` line to include arguments:

```ini
ExecStart=/usr/local/bin/TgBot++_main --TOKEN your_token_here
```

Note: Command-line arguments with sensitive data (like tokens) will be visible in process listings.
