################## libgit2 for accessing git in C code ##############
include(cmake/libgit2.cmake)
#####################################################################

################# TgBot Utilities (generic) Library #################
add_library_san(
  TgBotUtils SHARED
  src/ConfigManager.cpp
  src/ConfigManager_${TARGET_VARIANT}.cpp
  src/GitData.cpp
  src/libos/libfs.cpp
  src/libos/libfs_${TARGET_VARIANT}.cpp)

target_link_libraries(TgBotUtils ${Boost_LIBRARIES} ${LIBGIT2_LIBS})
target_link_lib_if_windows(TgBotUtils shlwapi)
#####################################################################