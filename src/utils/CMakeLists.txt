find_package(Boost COMPONENTS program_options REQUIRED)
find_package(libgit2 REQUIRED)

################# TgBot Utilities (generic) Library #################
add_my_library(
  NAME Utils
  SRCS
    ConfigManager.cpp
    Env_${TARGET_VARIANT}.cpp
    GitData.cpp
    CommandLine.cpp
    ResourceManager.cpp
    libfs.cpp
    libfs_${TARGET_VARIANT}.cpp
  LIBS Boost::program_options ${LIBGIT2_LIBRARIES}
  LIBS_WIN32 shlwapi
)