# GitHub Copilot Instructions for TgBot++

## Project Overview
TgBot++ is a Telegram bot implementation written in C++ and C, primarily used for learning purposes and personal use. The project uses modern C++23 standards and integrates with the Telegram Bot API.

## Repository Structure
- **src/**: Main source code directory
  - **command_modules/**: Bot command implementations
  - **socket/**: Socket-based API for external communication
  - **database/**: Database backend implementations (SQLite, Protobuf)
  - **web/**: Web server components
  - **api/**: API interfaces
  - **include/**: Public headers
  - **utils/**: Utility functions
  - **ml/**: Machine learning components (optional)
- **tests/**: Test suite using GTest
- **www/**: Web interface resources
- **resources/**: Static resources
- **CMake/**: CMake configuration files

## Build System
- **Build Tool**: CMake (minimum version 3.14)
- **Package Manager**: vcpkg
- **Build Configuration**: Use CMakePresets.json for standard configurations
- **Standard Commands**:
  ```bash
  cmake -B build -S . -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
  cmake --build build
  cmake --build build --target optional_all  # Build optional components
  ```

## CMake Options
The project uses several CMake options to control features:

**Main Options** (defined in root CMakeLists.txt):
- `TGBOTCPP_BUILD_TESTS`: Build the test suite (default: OFF)
- `TGBOTCPP_RUST_MODULES`: Enable Rust language command modules (default: OFF)

**Module-Specific Options** (defined in subdirectories):
- `TGBOTCPP_LUA_MODULES`: Enable Lua language command modules (default: ON, defined in src/api/CMakeLists.txt)
- `TGBOTCPP_SOCKET_PACKET_VERBOSE`: Enable verbose hex-view logging for socket packets (default: OFF, defined in src/socket/CMakeLists.txt)
- `ENABLE_LLM`: Enable LLM command module (requires CUDA toolkit and NVIDIA GPU) (default: OFF, defined in src/command_modules/CMakeLists.txt)

## Language Standards
- **C++**: C++23 standard (required)
  - Extensions are disabled (`CMAKE_CXX_EXTENSIONS OFF`)
  - Standard is enforced (`CMAKE_CXX_STANDARD_REQUIRED ON`)
- **C**: Modern C standard for the popen_wdt library

## Code Style and Formatting
- **Code Formatter**: clang-format with Google style base
  - Configuration: `.clang-format`
  - IndentWidth: 4 spaces
  - TabWidth: 4 spaces
  - UseTab: Never
- **Linter**: clang-tidy
  - Configuration: `.clang-tidy`
  - Most checks enabled with specific exclusions for abseil, altera, android, fuchsia, google, llvm, zircon
  - Trailing return types not enforced
- **Formatting Command**: Use clang-format to format code before committing

## Key Dependencies
The project uses vcpkg for dependency management. Major dependencies include:
- **TgBot-cpp**: Core Telegram bot library
- **Abseil**: Logging and common utilities
- **cpp-httplib**: HTTP server support
- **sqlite3**: Database backend
- **protobuf**: Alternative database backend
- **libgit2**: Git operations
- **GTest**: Testing framework
- **Sol2**: Lua integration
- **CppTrace**: Backtrace support
- **OpenCV4**: Image processing
- **Boost**: Various components (asio, system, program-options, units)

## Testing
- **Framework**: Google Test (GTest)
- **Test Location**: `tests/` directory
- **Running Tests**: 
  ```bash
  cd build
  ctest --output-on-failure --timeout 30
  ```
- **Test Files**: Use suffix `Test.cpp` for test files
- **Mock Objects**: Located in `tests/mocks/`

## Configuration
The bot uses a configuration file with the following options:
- `TOKEN`: Telegram bot token (required)
- `LOG_FILE`: Log file path
- `DATABASE_CFG`: Database configuration format: "type:filename" (type: sqlite or protobuf)
- `SOCKET_CFG`: Socket API configuration (ipv4 or ipv6, omit to disable)
- `GITHUB_TOKEN`: GitHub token for private repository access
- `OPTIONAL_COMPONENTS`: Comma-separated list (e.g., "webserver,datacollector")
- `BUILDBUDDY_API_KEY`: BuildBuddy API key for Android RBE

## Multi-Language Support
The project includes components in multiple languages:
- **C/C++**: Main codebase
- **Kotlin**: Android socket client app
- **JavaScript**: Web interface
- **PHP**: Web backend
- **HTML/CSS/SCSS**: Web UI
- **Lua**: Optional command modules
- **Rust**: Optional command modules

## Best Practices
1. **Memory Safety**: Use modern C++ practices (smart pointers, RAII)
2. **Error Handling**: Properly handle exceptions and return codes
3. **Logging**: Use Abseil logging framework consistently
4. **Testing**: Write tests for new functionality using GTest
5. **Documentation**: Document public APIs and complex logic
6. **Code Organization**: Follow the existing directory structure
7. **Platform Support**: Ensure changes work on Linux, macOS, and Windows
8. **Optional Features**: Keep optional components behind CMake flags

## GitHub Actions
The project uses CI/CD workflows:
- **linux_test.yml**: Linux builds with GCC and Clang
- **macos_test.yml**: macOS builds
- **windows_test.yml**: Windows builds
- All workflows build and run tests
- Rust module support is tested separately

## Socket API
The project includes a socket-based API for external communication:
- Documentation: `src/socket/README.api.md`
- Supports IPv4 and IPv6
- Used by the Android client application
- Verbose logging available via CMake option

## Special Considerations
1. **Position Independent Code**: PIE support is enabled when available
2. **Windows Support**: Uses dlfcn-win32 for runtime command loading
3. **VCPKG Dependencies**: DLLs are automatically copied to executable directory
4. **Submodules**: Project uses git submodules; ensure they're initialized
5. **Optional Components**: Web server and data collector are optional features

## When Making Changes
1. Format code with clang-format before committing
2. Run clang-tidy to check for issues
3. Build with all supported compilers (GCC, Clang, MSVC)
4. Run the test suite to ensure no regressions
5. Update tests if adding new functionality
6. Update documentation if changing APIs or configuration options
7. Consider platform compatibility (Linux, macOS, Windows)
8. Check that optional features remain behind their respective CMake flags
