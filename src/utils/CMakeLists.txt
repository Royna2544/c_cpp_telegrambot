find_package(Boost CONFIG 1.70 COMPONENTS program_options REQUIRED)

################# TgBot Utilities (generic) Library #################

tgbot_library(
  NAME Utils
  SRCS
    ConfigManager.cpp
    Env_${TARGET_VARIANT}.cpp
    CommandLine.cpp
    ResourceManager.cpp
    libfs_${TARGET_VARIANT}.cpp
  LIBS Boost::program_options
  LIBS_WIN32 shlwapi
  DEFS TGBOT_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
)
