find_package(Boost COMPONENTS program_options REQUIRED)
find_package(libgit2 REQUIRED)
################# TgBot Utilities (generic) Library #################
add_my_library(
  NAME Utils
  SRCS
    src/ConfigManager.cpp
    src/ConfigManager_${TARGET_VARIANT}.cpp
    src/GitData.cpp
    src/libos/libfs.cpp
    src/libos/libfs_${TARGET_VARIANT}.cpp
    src/libos/libsighandler_impl.cpp
    src/libos/libsighandler_${TARGET_VARIANT}.cpp
    src/ResourceManager.cpp
  LIBS Boost::program_options ${LIBGIT2_LIBRARIES} TgBotCommandLine
  LIBS_WIN32 shlwapi
)
#####################################################################