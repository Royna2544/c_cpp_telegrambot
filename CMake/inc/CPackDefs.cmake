
# Set CPack configurations
set(CPACK_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A Telegram bot coded in C++")
set(CPACK_PACKAGE_CONTACT "roynatech@gmail.com")
set(CPACK_GENERATOR "ZIP;TGZ")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "TgBot C++ server ${CPACK_PACKAGE_VERSION}")

if(WIN32)
  # NSIS
  list(APPEND CPACK_GENERATOR NSIS)
  set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
  set(CPACK_NSIS_MUI_ICON ${CMAKE_SOURCE_DIR}/resources/photo/icon.ico)
  set(CPACK_NSIS_MUI_UNIICON ${CMAKE_SOURCE_DIR}/resources/photo/uninstall.ico)
  set(CPACK_NSIS_DISPLAY_NAME "The Glider Project")
  set(CPACK_NSIS_WELCOME_TITLE "Welcome to The Glider Project Installer")
  set(CPACK_NSIS_BRANDING_TEXT "Created with CMake ${CMAKE_VERSION}")
  set(CPACK_NSIS_IGNORE_LICENSE_PAGE ON)
  set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\${PROJECT_NAME}_main.exe")
  set(CPACK_NSIS_MANIFEST_DPI_AWARE TRUE)

  # Install Visual C++ Redistributable if needed
  set(CMAKE_INSTALL_UCRT_LIBRARIES TRUE)
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION bin)
  set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT AppMain)
  include(InstallRequiredSystemLibraries)

  # Find the VC redistributable installer to bundle
  set(_vcredist_candidates
    "$ENV{VCToolsRedistDir}/vc_redist.x64.exe"
    "$ENV{VCINSTALLDIR}/Redist/MSVC/v143/vc_redist.x64.exe"
  )
  set(_vcredist_path "")
  foreach(_candidate IN LISTS _vcredist_candidates)
    if(EXISTS "${_candidate}")
      set(_vcredist_path "${_candidate}")
      break()
    endif()
  endforeach()

  if(NOT _vcredist_path)
    # Search common Visual Studio paths
    file(GLOB _vcredist_glob
      "C:/Program Files/Microsoft Visual Studio/*/*/VC/Redist/MSVC/*/vc_redist.x64.exe"
    )
    list(GET _vcredist_glob 0 _vcredist_path)
  endif()

  if(_vcredist_path)
    message(STATUS "Found VC redistributable: ${_vcredist_path}")
    install(PROGRAMS "${_vcredist_path}" DESTINATION bin COMPONENT AppMain)
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "
      ExecWait '\\\"$INSTDIR\\\\bin\\\\vc_redist.x64.exe\\\" /install /quiet /norestart'
      Delete '\\\"$INSTDIR\\\\bin\\\\vc_redist.x64.exe\\\"'
    ")
  else()
    message(WARNING "VC redistributable not found; installer will not include vcredist")
  endif()

  # Start Menu shortcut for the main application
  set(CPACK_NSIS_CREATE_ICONS_EXTRA
    "CreateShortCut '$SMPROGRAMS\\\\$STARTMENU_FOLDER\\\\Glider.lnk' '$INSTDIR\\\\bin\\\\${PROJECT_NAME}_main.exe'"
  )
  set(CPACK_NSIS_DELETE_ICONS_EXTRA
    "Delete '$SMPROGRAMS\\\\$START_MENU\\\\Glider.lnk'"
  )
endif()

if (LINUX)
  # DEB
  list(APPEND CPACK_GENERATOR DEB)
  set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Soo Hwan Na <roynatech@gmail.com>")
  set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
  set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libstdc++6")
  set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
endif()

set(CPACK_PACKAGE_FILE_NAME 
    "${PROJECT_NAME}-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}"
)

# Include CPack
include(CPack)

cpack_add_component_group(
  OptionalCli
  DISPLAY_NAME "Optional Cli tools"
  DESCRIPTION "Additional CLIs."
  EXPANDED # Expand the group by default in the installer UI
)

cpack_add_component(
  AppMain
  DISPLAY_NAME "Main Application"
  DESCRIPTION "The main server application."
  REQUIRED # Ensures this component is always installed
)

cpack_add_component(
  AppSocket
  DISPLAY_NAME "Socket Utils"
  DESCRIPTION "Socket connection clients."
  GROUP "OptionalCli")

cpack_add_component(
  AppDatabase
  DISPLAY_NAME "Database Utils"
  DESCRIPTION "Database manipulation utils."
  GROUP "OptionalCli")
