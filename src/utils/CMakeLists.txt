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
  NAME Utils-Resource
  SRCS
    ResourceManager.cpp
)
target_link_libraries(Utils-Resource PUBLIC Utils)

tgbot_library(
  NAME Utils
  SRCS 
    CommandLine.cpp
    ConfigManager.cpp
    Env_${TARGET_VARIANT}.cpp
    libfs_${TARGET_VARIANT}.cpp
)
target_link_libraries(Utils PRIVATE $<$<BOOL:${WIN32}>:shlwapi> Boost::program_options ${CMAKE_DL_LIBS})
target_compile_definitions(Utils PRIVATE ${UTILS_CMAKE_DEFINITIONS})