find_package(Boost CONFIG 1.70 COMPONENTS program_options REQUIRED)

################# TgBot Utilities (generic) Library #################

if (TGBOTCPP_CROSS_COMPILE_INSTALL_PATH)
  set(UTILS_CMAKE_DEFINITIONS
    INSTALL_PREFIX="${TGBOTCPP_CROSS_COMPILE_INSTALL_PATH}"
  )
else()
  set(UTILS_CMAKE_DEFINITIONS
    INSTALL_PREFIX="${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}"
  )
endif()

tgbot_library(
  NAME Utils
  SRCS
    ConfigManager.cpp
    Env_${TARGET_VARIANT}.cpp
    CommandLine.cpp
    ResourceManager.cpp
    libfs_${TARGET_VARIANT}.cpp
    LIBS Boost::program_options $<$<BOOL:${WIN32}>:shlwapi>
    DEFS ${UTILS_CMAKE_DEFINITIONS}
)
