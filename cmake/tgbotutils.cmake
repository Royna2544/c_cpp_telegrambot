################## libgit2 for accessing git in C code ##############
include(cmake/libgit2.cmake)
#####################################################################

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
  LIBS ${Boost_LIBRARIES} ${LIBGIT2_LIBS}
  LIBS_WIN32 shlwapi
)
#####################################################################