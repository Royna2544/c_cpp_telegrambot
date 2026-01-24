# GitHub Copilot Instructions for Glider

## Project Overview
Glider is a Telegram bot implementation written in C++ and C, primarily used for learning purposes and personal use. The project uses modern C++20/C++23 standards and integrates with the Telegram Bot API.

## Repository Structure
- **src/**: Main source code directory
  - **command_modules/**: Bot command implementations (compiler, builder, support)
    - **compiler/**: Compiler command modules
    - **builder/**: Build system components including kernel builder
    - **support/**: Support libraries (popen_wdt, progress)
  - **socket/**: Socket-based API for external communication
    - **server/**: Server-side implementation (SocketInterface, PacketDispatcher, CommandHandlers)
    - **client/**: Client-side implementation
    - **shared/**: Shared utilities (FileHelperNew)
  - **database/**: Database backend implementations (SQLite, Protobuf)
  - **web/**: Web server components
  - **api/**: API interfaces
  - **include/**: Public headers
  - **utils/**: Utility functions
  - **ml/**: Machine learning components (optional)
  - **random/**: Random number generation with hardware support
  - **imagep/**: Image processing utilities
  - **logging/**: Logging infrastructure
  - **libos/**: OS-specific library wrappers
  - **third-party/**: Third-party dependencies
    - **tgbot-cpp/**: Telegram bot library
    - **corrosion/**: Rust integration support
- **tests/**: Test suite using GTest
- **www/**: Web interface resources (PHP, JavaScript, SCSS)
- **resources/**: Static resources
- **CMake/**: CMake configuration files

## Build System
- **Build Tool**: CMake (minimum version 3.14)
- **Current Version**: CMake 4.1.1 (MSVC variant)
- **Package Manager**: vcpkg
- **Build Generator**: Ninja
- **Build Configuration**: Use CMakePresets.json for standard configurations
- **Standard Commands**:
  - cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
  - cmake --build build
  - cmake --build build --target optional_all

## CMake Options
The project uses several CMake options to control features:

**Main Options** (defined in root CMakeLists.txt):
- TGBOTCPP_BUILD_TESTS: Build the test suite (default: OFF)
- TGBOTCPP_RUST_MODULES: Enable Rust language command modules (default: OFF)
- BUILD_SHARED_LIBS: Build some dependent libraries as shared (default: OFF)

**Module-Specific Options** (defined in subdirectories):
- TGBOTCPP_LUA_MODULES: Enable Lua language command modules (default: ON, defined in src/api/CMakeLists.txt)
- TGBOTCPP_SOCKET_PACKET_VERBOSE: Enable verbose hex-view logging for socket packets (default: OFF, defined in src/socket/CMakeLists.txt)
- ENABLE_LLM: Enable LLM command module (requires CUDA toolkit and NVIDIA GPU) (default: OFF, defined in src/command_modules/CMakeLists.txt)

## Language Standards
- **C++**: C++20 standard (set in CMakeLists.txt), with C++23 features used where available
  - Extensions are disabled (CMAKE_CXX_EXTENSIONS OFF)
  - Standard is enforced (CMAKE_CXX_STANDARD_REQUIRED ON)
  - Requires C++20 jthread and stop_token support (mandatory)
- **C**: Modern C standard for the popen_wdt library

## CMake Custom Functions
The project defines helper functions for consistent target configuration:

1. **tgbot_common_target**: Base function for common target properties
   - Sets include directories, compile definitions, and link libraries
   - Configures RPATH for Unix systems ($ORIGIN/../lib, $ORIGIN/..)
   - Applies LTO when supported
   - Sets output directories for binaries and libraries

2. **tgbot_library**: Creates shared or static libraries
   - Supports STATIC, NO_LIBPREFIX, NO_HIDE options
   - Automatically generates export headers for shared libraries
   - Windows: Uses Telegram.Bot. prefix, Unix: Uses TgBot prefix
   - Sets symbol visibility to hidden by default

3. **tgbot_exe**: Creates executables with automatic main wrapper
   - Supports OPTIONAL, TEST, NO_MAIN_WRAPPER options
   - Optional executables added to optional_all target
   - Test executables get test_ prefix and auto-registered with CTest
   - Requires RELATION parameter (Main, Socket, or Database)

4. **command_module**: Creates dynamically loadable command modules
   - Supports platform-specific builds (Unix, Linux)
   - Package dependency gating
   - Rust module support
   - Forces lib prefix on all platforms
   - Output to lib/modules/ directory

5. **multi_command_module**: Creates multiple command modules from single source

## Code Style and Formatting
- **Code Formatter**: clang-format with Google style base
  - Configuration: .clang-format
  - IndentWidth: 4 spaces
  - TabWidth: 4 spaces
  - UseTab: Never
- **Linter**: clang-tidy
  - Configuration: .clang-tidy
  - Most checks enabled with specific exclusions for abseil, altera, android, fuchsia, google, llvm, zircon
  - Trailing return types not enforced
- **Formatting Command**: Use clang-format to format code before committing

## Key Dependencies
The project uses vcpkg for dependency management. Major dependencies include:
- **TgBot-cpp**: Core Telegram bot library (third-party subdirectory)
- **Abseil**: Logging and common utilities
- **cpp-httplib**: HTTP server support
- **sqlite3**: Database backend
- **protobuf**: Alternative database backend
- **libgit2**: Git operations (used in build info generation)
- **GTest**: Testing framework
- **Sol2**: Lua integration
- **CppTrace**: Backtrace support
- **OpenCV4**: Image processing
- **Boost**: Various components (asio, system, program-options, units)
- **fmt**: Format library (header-only mode)
- **nlohmann_json**: JSON parsing
- **CURL**: HTTP client
- **fruit**: Dependency injection framework
- **dlfcn-win32**: Windows DLL loading support

## Compiler Features
- **Link-Time Optimization (LTO)**: Enabled in Release builds when supported
- **Position Independent Code (PIE)**: Enabled when supported
- **C++20 jthread/stop_token**: Mandatory support required
  - Clang: Uses -fexperimental-library flag if needed
  - Linker: Prefers LLD for Clang builds
- **Ccache**: Automatic detection and usage for faster builds
- **MSVC**: UTF-8 encoding enforced (/utf-8), NOMINMAX defined

## Testing
- **Framework**: Google Test (GTest)
- **Test Location**: tests/ directory
- **Running Tests**: 
  - cd build
  - ctest --output-on-failure --timeout 30
- **Test Files**: Use suffix Test.cpp for test files
- **Test Executables**: Automatically registered with CTest
- **Mock Objects**: Located in tests/mocks/

## Configuration
The bot uses a configuration file with the following options:
- TOKEN: Telegram bot token (required)
- LOG_FILE: Log file path
- DATABASE_CFG: Database configuration format: "type:filename" (type: sqlite or protobuf)
- SOCKET_CFG: Socket API configuration (ipv4 or ipv6, omit to disable)
- GITHUB_TOKEN: GitHub token for private repository access
- OPTIONAL_COMPONENTS: Comma-separated list (e.g., "webserver,datacollector")
- BUILDBUDDY_API_KEY: BuildBuddy API key for Android RBE

## Multi-Language Support
The project includes components in multiple languages:
- **C/C++**: Main codebase (primary)
- **Kotlin**: Android socket client app
- **JavaScript**: Web interface
- **PHP**: Web backend and HTML generation
- **HTML/CSS/SCSS**: Web UI styling
- **Lua**: Optional command modules (dynamically loaded)
- **Rust**: Optional command modules (via Corrosion)

## Socket Interface Architecture
The socket API provides external communication capabilities:

**Core Components**:
- **SocketInterfaceTgBot**: Main socket interface implementing ThreadRunner
- **Session Management**: Token-based authentication with replay attack prevention
  - Session struct: Contains session_key, last_nonce, and expiry
  - Prevents replay attacks using nonce tracking
- **Chunked File Transfer**: Supports large file transfers with SHA256 verification
  - ChunkedTransferSession: Tracks transfer state including buffer, chunk_size, expected_chunk_index
  - Mutex-protected transfer_sessions map

**Command Handlers**:
- handle_WriteMsgToChatId: Send messages to chats
- handle_SendFileToChatId: Send files to chats
- handle_CtrlSpamBlock: Control spam blocking
- handle_ObserveChatId: Observe specific chats
- handle_ObserveAllChats: Observe all chats
- handle_TransferFile: Single-shot file transfer
- handle_TransferFileBegin: Initialize chunked transfer
- handle_TransferFileChunk: Process file chunk
- handle_TransferFileEnd: Finalize chunked transfer
- handle_TransferFileRequest: Request file from server
- handle_GetUptime: Query server uptime
- handle_OpenSession: Create new session
- handle_CloseSession: Terminate session

**Dependencies**:
- TgBotApi: Telegram bot API interface
- ChatObserver: Chat observation functionality
- SpamBlockBase: Spam blocking control
- SocketFile2DataHelper: File data conversion
- ResourceProvider: Resource management

**Features**:
- Documentation: src/socket/README.api.md
- Supports IPv4 and IPv6
- Used by the Android client application
- Verbose logging available via TGBOTCPP_SOCKET_PACKET_VERBOSE option

## Random Number Generation
The project uses a tiered random number generation system:
1. **RDRand** (x86): Hardware RNG using RDRAND instruction (Intel/AMD CPUs)
2. **KernelRand** (Linux/macOS): OS-provided hardware RNG interface
3. **std::mt19937** (fallback): Standard C++ pseudo-RNG

Selection is automatic at runtime based on hardware/OS support.

## Command Module System
Command modules are dynamically loadable plugins:

**Configuration**:
- **Prefix**: cmd_ for internal naming, lib for output files
- **Output Directory**: lib/modules/
- **Platform Gating**: Supports Unix, Linux platform restrictions
- **Package Dependencies**: Optional external package requirements

**Module Types**:
- Single-source modules (one command per source)
- Multi-source modules (multiple commands from one source)

**Available Modules**:
- **Text Commands**: alive, start, decho, cmd, spam, log
- **Scripting**: bash, ubash, c, cpp, py (with Python3)
- **Database**: database, saveid, setowner
- **Randomization**: decide, flash, possibility, randsticker
- **File Operations**: fileid, copystickers, rotatepic
- **Network**: up, down
- **Advanced**: stringcalc (Rust), ask (LLM with CUDA)
- **Utilities**: delay

## Build Info Generation
The build system automatically generates Git build information:
- Commit ID (git rev-parse HEAD)
- Commit message (git log -1 --pretty=%B)
- Repository origin URL (git remote get-url origin)
- Build timestamp (UTC)
- Generated in: src/include/GitBuildInfo.hpp
- Used in: resources/about.html

## Best Practices
1. **Memory Safety**: Use modern C++ practices (smart pointers, RAII)
2. **Error Handling**: Properly handle exceptions and return codes
3. **Logging**: Use Abseil logging framework consistently
4. **Testing**: Write tests for new functionality using GTest
5. **Documentation**: Document public APIs and complex logic
6. **Code Organization**: Follow the existing directory structure
7. **Platform Support**: Ensure changes work on Linux, macOS, and Windows
8. **Optional Features**: Keep optional components behind CMake flags
9. **Symbol Visibility**: Shared libraries hide symbols by default (CXX_VISIBILITY_PRESET hidden)
10. **RPATH Configuration**: Unix executables use $ORIGIN/../lib and $ORIGIN/.. for library discovery
11. **Dependency Injection**: Use Fruit framework with APPLE_EXPLICIT_INJECT macro
12. **Thread Management**: Use ThreadRunner interface for managed threads with stop_token support

## GitHub Actions
The project uses CI/CD workflows:
- **linux_test.yml**: Linux builds with GCC and Clang
- **macos_test.yml**: macOS builds
- **windows_test.yml**: Windows builds (with MSVC)
- All workflows build and run tests
- Rust module support is tested separately

## Packaging (CPack)
The project uses CPack for distribution:

**Generators**: ZIP, TGZ, NSIS (Windows)

**Components**:
- AppMain: Main application (required)
- AppSocket: Socket utilities (optional, part of OptionalCli group)
- AppDatabase: Database utilities (optional, part of OptionalCli group)
- CommandModules: Group containing individual command modules
- Command_{NAME}: Individual command module components

**NSIS Settings** (Windows):
- Custom icons for installer/uninstaller
- DPI awareness enabled
- License page skipped
- Uninstall before install enabled

## Special Considerations
1. **Position Independent Code**: PIE support is enabled when available
2. **Windows Support**: 
   - Uses dlfcn-win32 for runtime command loading
   - UTF-8 encoding enforced (/utf-8)
   - NOMINMAX defined to avoid macro conflicts
   - Custom library prefix: Telegram.Bot.
3. **VCPKG Dependencies**: DLLs are automatically copied to executable directory and install location
4. **Submodules**: Project uses git submodules; ensure they're initialized
5. **Optional Components**: 
   - Web server and data collector are optional features
   - optional_all target builds all optional components
6. **Resource Management**: Resources copied to both build and install directories (share/${PROJECT_NAME})
7. **Dependency Injection**: Uses Fruit framework with APPLE_EXPLICIT_INJECT macro for constructor injection
8. **Thread Management**: Custom thread management system with ThreadRunner, ManagedThreads, and stop_token support

## When Making Changes
1. Format code with clang-format before committing
2. Run clang-tidy to check for issues
3. Build with all supported compilers (GCC, Clang, MSVC)
4. Run the test suite to ensure no regressions
5. Update tests if adding new functionality
6. Update documentation if changing APIs or configuration options
7. Consider platform compatibility (Linux, macOS, Windows)
8. Check that optional features remain behind their respective CMake flags
9. Verify that command modules are properly registered with CPack components
10. Test RPATH settings on Unix platforms
11. Ensure symbol visibility is correctly configured for shared libraries
12. Test dependency injection with Fruit framework if modifying constructors
