# A Telegram bot, mostly used by my own uses and learning purposes

## Status
[![Build Glider (Multi Platform)](https://github.com/Royna2544/c_cpp_telegrambot/actions/workflows/build.yml/badge.svg)](https://github.com/Royna2544/c_cpp_telegrambot/actions/workflows/build.yml)

[![Build Glider builder-rs](https://github.com/Royna2544/c_cpp_telegrambot/actions/workflows/rust.yml/badge.svg)](https://github.com/Royna2544/c_cpp_telegrambot/actions/workflows/rust.yml)

[![CodeQL Analysis](https://github.com/Royna2544/c_cpp_telegrambot/actions/workflows/codeql.yml/badge.svg)](https://github.com/Royna2544/c_cpp_telegrambot/actions/workflows/codeql.yml)

## Used languages
| C | C++ | Kotlin | Javascript | SQLite3 | HTML5 | CSS | SCSS | PHP |
|-- | --- | ------ | ---------- | --- | ----- | --- | ---- | --- |
| <img src="www/resources/devicons/c-original.svg" title="C"  alt="C" width="55" height="55"/>|<img src="www/resources/devicons/cplusplus-original.svg" title="C++"  alt="C++" width="55" height="55"/>|<img src="www/resources/devicons/kotlin-original.svg" title="Kotlin"  alt="Kotlin" width="55" height="55"/>|<img src="www/resources/devicons/javascript-original.svg" title="Javascript" alt="Javascript" width="55" height="55"/>|<img src="https://github.com/devicons/devicon/blob/master/icons/sqlite/sqlite-original.svg" title="SQLite"  alt="SQLite" width="55" height="55"/>|<img src="https://github.com/devicons/devicon/blob/master/icons/html5/html5-original.svg" title="HTML5"  alt="HTML5" width="55" height="55"/>|<img src="https://github.com/devicons/devicon/blob/master/icons/css3/css3-original.svg" title="CSS"  alt="CSS" width="55" height="55"/>|<img src="www/resources/devicons/sass-original.svg" title="SCSS"  alt="SCSS" width="55" height="55"/>| <img src="www/resources/devicons/php-original.svg" title="PHP"  alt="PHP" width="55" height="55"/>|
| popen_wdt lib | main C++ code | Android socket client app | Webpage | Database support | Webpage | Webpage | Webpage | Webpage |

## Used external libraries
- Abseil C++ common library - Used for logging - [Link](https://github.com/abseil/abseil-cpp)
- C++ HTTP library (cpp-httplib) - Used for website server support - [Link](https://github.com/yhirose/cpp-httplib)
- CppTrace - C++ Backtrace Library - [Link](https://github.com/jeremy-rifkin/cpptrace)
- cryptopp - Used for hashing and encryption - [Link](https://github.com/abdes/cryptopp-cmake)
- CURL - Used for HTTP requests - [Link](https://github.com/curl/curl)
- dlfcn Win32 support - Used for runtime loader for commands (Windows only) - [Link](https://github.com/dlfcn-win32/dlfcn-win32)
- fmt - Format library (header-only mode) - [Link](https://github.com/fmtlib/fmt)
- Fruit - C++ Dependency Injection Framework - [Link](https://github.com/google/fruit)
- gRPC - C++ gRPC implementation for remote services - [Link](https://grpc.io/)
- GTest - Test framework for C++ codes - [Link](https://github.com/google/googletest)
- libxml2 - Used for XML parsing in StringResLoader - [Link](https://gitlab.gnome.org/GNOME/libxml2)
- llama.cpp - Used for local LLM support - [Link](https://github.com/ggml-org/llama.cpp)
- Lua - Lua language integration (for command modules) - [Link](https://github.com/lua/lua)
- nlohmann_json - Used for JSON parsing and serialization - [Link](https://github.com/nlohmann/json)
- OpenCV - Used for image/video processing - [Link](https://github.com/opencv/opencv)
- Protobuf - Used for alternative database backend and gRPC - [Link](https://github.com/protocolbuffers/protobuf)
- Sol2 - C++ and Lua integration - [Link](https://github.com/ThePhD/sol2)
- SQLite3 - Used for one database backend, for whitelist/blacklist saving support
- stduuid - Used for generating UUIDs - [Link](https://github.com/mariusbancila/stduuid/)
- TgBot-cpp - Core library for being a Telegram bot - [Link](https://github.com/Royna2544/tgbot-cpp)

### Image Processing Libraries
- libpng - PNG image format support
- libjpeg-turbo - JPEG image format support
- libwebp - WebP image format support

### Build Tools & Integration
- Corrosion - Rust and CMake integration - [Link](https://github.com/corrosion-rs/corrosion)
- Git - Used for build information generation (commit ID, message, origin)
- Rust - Used for some command modules and the builder-rs gRPC server - [Link](https://www.rust-lang.org/)

## Goals
- There is no goal - I will just add stuff whichever I could and learn those while writing code for it

## TODOs
- See TODO file, though its not really updated

## Cmake options
# CMake options has a common prefix of 'TGBOTCPP_'
- TGBOTCPP_BUILD_TESTS: Build the test suite (default: OFF)
- TGBOTCPP_RUST_MODULES: Enable and build command modules written with Rust language (default: OFF)
- TGBOTCPP_LUA_MODULES: Enable support for command module written with Lua language (default: ON)
- TGBOTCPP_ENABLE_LOCAL_LLM: Enable support for local LLM framework (Requires compatible hardware, e.g. NVIDIA GPU) (default: OFF)
- TGBOTCPP_CROSS_COMPILING: Whether we are cross-compiling, can be auto detected or be manually set
- TGBOTCPP_AUTODETECT_CROSS_COMPILING: Auto detect TGBOTCPP_CROSS_COMPILING via CMAKE_HOST_CMAKE_SYSTEM_PROCESSOR and CMAKE_SYSTEM_PROCESSOR
- TGBOTCPP_CROSS_COMPILE_INSTALL_PATH: Path that the package is installed to (i.e. CMAKE_INSTALL_PREFIX). Default: /usr/bin (assuming debian systems)
- TGBOTCPP_CROSS_COMPILE_CC: C Compiler that is present on remote system, can be omitted - then the following backend is disabled.
- TGBOTCPP_CROSS_COMPILE_CXX: C++ Compiler that is present on remote system, can be omitted - then the following backend is disabled.
- TGBOTCPP_CROSS_COMPILE_PYTHON: Python interpreter that is present on remote system, can be omitted - then the following backend is disabled.

## Config file options
### Section Main
- Token: Telegram bot token
- LogFile: Log file path
- GitHubToken: GitHub token (Used for private repo access)
- OptionalComponents: Enable optional components. Comma-separated list of components to enable. Supported components: "webserver", "datacollector"
- BuildBuddyApiKey: BuildBuddy API key for Android RBE

### Section Database
- FilePath: Database file path
- Type: Database type (sqlite or protobuf)

### Section LLM
- BackendType: LLM backend type (local, localnet)
- ModelLocation: LLM location (file path or network address)
- AuthKey: LLM authentication key (if required)

### Section Network
- PrimarySocketUrl: Primary socket URL that is binded to. e.g. 192.168.0.X:239 or unix://path/to/file
- SecondarySocketUrl: Secondary socket URL
- LoggingSocketUrl: Logging socket URL
- ApiServer: Custom Telegram API server URL
- ApiServerFilePathRemovePrefix: Prefix to remove from Telegram #getFile file paths when using a custom API server
- ApiServerFilePathAppendPrefix: Prefix to append to Telegram #getFile file paths when using a custom API server
- BuilderRSServer: Builder-RS gRPC server address