set(CMDMODULE_PREFIX cmd_)

set(CMDMODULE_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

cpack_add_component_group(
  CommandModules
  DISPLAY_NAME "Command Modules"
  DESCRIPTION "Command Modules to install."
  EXPANDED # Expand the group by default in the installer UI
)

function(command_module)
  cmake_parse_arguments(
    CMD
    "RUST" # options
    "NAME;PLATFORM;PACKAGE" # one-value keywords
    "SRCS;LIBS" # multi-value keywords
    ${ARGN})

  # ---------- validation / feature gates ----------
  if(NOT CMD_SRCS OR NOT CMD_NAME)
    message(FATAL_ERROR "SRCS and NAME must be specified")
  endif()

  # 1. Rust support gate
  if(CMD_RUST AND NOT ENABLE_RUST_SUPPORT)
    message(STATUS "Command ${CMD_NAME} disabled (no Rust support)")
    return()
  endif()

  # 1. Platform gate  (extend as needed)
  if(CMD_PLATFORM STREQUAL "Unix" AND NOT UNIX)
    message(STATUS "Command ${CMD_NAME} disabled (not Unix)")
    return()
  elseif(CMD_PLATFORM STREQUAL "Linux" AND NOT (LINUX OR ANDROID))
    message(STATUS "Command ${CMD_NAME} disabled (not Linux)")
    return()
  elseif(CMD_PLATFORM AND NOT CMD_PLATFORM MATCHES "(Unix|Linux)")
    message(WARNING "Unknown platform selector: ${CMD_PLATFORM}")
  endif()

  # 1. Optional external package gate
  if(CMD_PACKAGE)
    find_package(${CMD_PACKAGE})
    if(NOT ${CMD_PACKAGE}_FOUND)
      message(STATUS "Package '${CMD_PACKAGE}' not found – disable ${CMD_NAME}")
      return()
    endif()
  endif()

  # ---------- assemble the real target ----------
  set(libname ${CMDMODULE_PREFIX}${CMD_NAME})

  add_library(${libname} SHARED ${CMD_SRCS})

  tgbot_common_target(
    ${libname}
    PUBLIC_LIBS
    Api
    absl::log
    ${CMD_LIBS}
    PRIVATE_INC
    $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:Random,INTERFACE_INCLUDE_DIRECTORIES>)

  # force the Unix-style “lib” prefix even on Windows
  set_target_properties(${libname} PROPERTIES PREFIX "lib")

  # Hide symbols by default
  set_target_properties(${libname} PROPERTIES
    CXX_VISIBILITY_PRESET        hidden
    VISIBILITY_INLINES_HIDDEN    YES)

  # override the default output directory just for this kind of module
  set_target_properties(
    ${libname}
    PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/modules
               RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/modules)

  # ---------- CPack component ----------
  cpack_add_component(
    Command_${CMD_NAME}
    DISPLAY_NAME "${CMD_NAME}"
    DESCRIPTION "Install command ${CMD_NAME}."
    GROUP "CommandModules")

  install(
    TARGETS ${libname}
    DESTINATION lib/modules
    COMPONENT Command_${CMD_NAME})
endfunction()

function(multi_command_module)
  cmake_parse_arguments(
    TGBOT_MULTI_CMDMODULE
    "" # Options
    "PLATFORM" # One-value keywords
    "SRCS;LIBS;PROVIDES" # Multiple-value keywords
    ${ARGN})
  if(NOT TGBOT_MULTI_CMDMODULE_PROVIDES)
    message(FATAL_ERROR "Must specify modules provided by this source")
  endif()
  foreach(module ${TGBOT_MULTI_CMDMODULE_PROVIDES})
    # Forward the arguments
    command_module(
      NAME
      ${module}
      SRCS
      ${TGBOT_MULTI_CMDMODULE_SRCS}
      LIBS
      ${TGBOT_MULTI_CMDMODULE_LIBS}
      PLATFORM
      ${TGBOT_MULTI_CMDMODULE_PLATFORM})
  endforeach()
endfunction()

add_subdirectory(support)
add_subdirectory(compiler)
add_subdirectory(builder)

multi_command_module(
  PROVIDES
  alive
  start
  SRCS
  alive.cpp
  LIBS
  absl::strings
  DBImpl
  Utils)

multi_command_module(
  PROVIDES
  bash
  ubash
  SRCS
  bash_impl.cpp
  LIBS
  Compiler)

multi_command_module(
  PROVIDES
  c
  cpp
  SRCS
  compilers_impl.cpp
  LIBS
  Compiler)

command_module(
  NAME
  py
  SRCS
  compilers_impl.cpp
  LIBS
  Compiler
  PACKAGE
  Python3)

command_module(NAME cmd SRCS cmd.cpp)

multi_command_module(
  PROVIDES
  database
  saveid
  SRCS
  database_impl.cpp
  LIBS
  DBImpl
  Random)

command_module(NAME decho SRCS decho.cpp)

command_module(NAME decide SRCS decide.cpp LIBS Random)

command_module(NAME delay SRCS delay.cpp)

command_module(NAME fileid SRCS fileid.cpp)

command_module(NAME flash SRCS flash.cpp LIBS Random)

command_module(NAME possibility SRCS possibility.cpp LIBS Random)

command_module(NAME randsticker SRCS randsticker.cpp LIBS Random)

command_module(NAME rotatepic SRCS rotatepic.cpp LIBS ImgProc)

command_module(NAME setowner SRCS setowner.cpp LIBS DBImpl)

command_module(NAME spam SRCS spam.cpp)

command_module(
  NAME
  stringcalc
  SRCS
  stringCalc.cpp
  LIBS
  stringcalc
  RUST)

command_module(
  NAME
  copystickers
  SRCS
  copystickers.cpp
  LIBS
  ImgProc
  Utils)

multi_command_module(PROVIDES up down SRCS updown.cpp)

command_module(NAME log SRCS log.cpp)
