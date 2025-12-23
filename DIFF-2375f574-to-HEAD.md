# Comparison summary: 2375f57433a322c05beae0967fd9d1683d958834 → HEAD (1bc4f0d0fdf1b2b6af8ac6c7cd0c235549604d1a)

- Base: 2023-10-25, HEAD: 2025-12-23 (roughly 2 years).
- Overall delta: 453 files changed (+44,371 insertions / -2,267 deletions).

## Build, tooling, and workflow changes
- Added clang-tidy/clangd configs, CMake presets, pyright settings, and CodeQL root marker; removed legacy Protobuf CMake helpers.
- Introduced `vcpkg.json` with expanded dependencies (fmt, libarchive, cpptrace, sol2, gtest, etc.) plus new scripts/resources; README and TODO files were added.
- GitHub hygiene updates: issue/PR templates added, CI workflows for Linux/macOS/Windows updated, and deploy workflow removed; third-party submodules refreshed (e.g., corrosion, tgbot-cpp).

## Source-code evolution
- Command modules greatly expanded (builder tasks for Android/kernel archives, compiler helpers, LLM/support modules), moving away from the old `lib`/`modules` layout.
- Socket subsystem extended with client PacketBuilder/SessionManager, Local/TCP/UDP contexts, and hexdump helpers.
- Utilities refactored: new ConfigManager, Env/ResourceManager, CommandLine helpers; legacy config/libutils files removed.
- Database/API adjustments: restart-format parser components added; database utilities/backends reshaped; proto artifacts trimmed (`proto` helpers removed, `tgbot.pb` cleared).
- Web server base received updates alongside new helper wiring.

## Testing
- New GoogleTest suite (21 files) now covers authorization, database backends, regex/popen watchdog helpers, socket data handlers, and many command modules with supporting mocks.

## Web/UI and resources
- New `www/` frontend introduced with PHP/JS/SCSS components (navigation, commands, about page, chip/desc boxes), assets, and icons.
- Resources expanded (photos/icons/mime data), alongside additional maintenance scripts.
