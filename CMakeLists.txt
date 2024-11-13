cmake_minimum_required(VERSION 3.15)
project(TgBot++ LANGUAGES CXX C)

####################### Set CMake Policy #######################
include(CMake/CompilerConfig.cmake)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

# Set RPATH for Unix-like systems
if(UNIX)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    set(CMAKE_BUILD_RPATH_USE_ORIGIN ON)
    set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib")
endif()
################ EXE NAMES STEMMED FROM PROJECT_NAME ################

############################## OPTIONS ##############################
option(ENABLE_RUST_SUPPORT "Enable rust support" OFF)
option(DISABLE_SANITIZERS "Disable all sanitizers (ASan, TSan, etc)" OFF)
option(DIRTY_ABSEIL_FIX "Enable dirty abseil linkage fix" OFF)
#####################################################################

if (WIN32)
  add_subdirectory(src/third-party/dlfcn-win32)
  set(CMAKE_DL_LIBS dl)
else()
  find_library(DL dl)
  add_library(dl SHARED IMPORTED)
  set_target_properties(dl PROPERTIES
    IMPORTED_LOCATION "${DL}"
  )
endif()

##################### Fill in readme with cmake #####################
string(TIMESTAMP TODAY "%Y-%m-%d")
configure_file(resources/about.html.in ${CMAKE_SOURCE_DIR}/resources/about.html)
#####################################################################

################## Declare common macros ####################
function(add_my_library)
  cmake_parse_arguments(TGBOT_LIBRARY
    "NO_LIBPREFIX;STATIC" # Options
    "NAME" # One-value keywords
    "SRCS;LIBS;LIBS_STATIC;PUBLIC_INC;PRIVATE_INC;DEPENDS;DEFS;LIBS_WIN32" # Multiple-value keywords
    ${ARGN}
  )
  if (NOT TGBOT_LIBRARY_SRCS OR NOT TGBOT_LIBRARY_NAME)
    message(FATAL_ERROR "Must specify src or name")
  endif()
  if (NOT TGBOT_LIBRARY_NO_LIBPREFIX)
    set(TGBOT_LIBRARY_NAME TgBot${TGBOT_LIBRARY_NAME})
  endif()
  if (TGBOT_LIBRARY_STATIC)
    add_library(${TGBOT_LIBRARY_NAME} STATIC ${TGBOT_LIBRARY_SRCS})
  else()
    add_library(${TGBOT_LIBRARY_NAME} SHARED ${TGBOT_LIBRARY_SRCS})
  endif()
  install(TARGETS ${TGBOT_LIBRARY_NAME} DESTINATION lib)
  set_target_properties(${TGBOT_LIBRARY_NAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  add_sanitizers(${TGBOT_LIBRARY_NAME})
    set(EXPORTCONF_DIR ${CMAKE_BINARY_DIR}/exportConfig/)
  if (NOT TGBOT_LIBRARY_STATIC)
    configure_file(${CMAKE_SOURCE_DIR}/src/include/LibraryExports.h.inc
      ${EXPORTCONF_DIR}${TGBOT_LIBRARY_NAME}Exports.h)
  endif()
  target_include_directories(${TGBOT_LIBRARY_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${TGBOT_LIBRARY_PUBLIC_INC} ${EXPORTCONF_DIR})
  target_include_directories(${TGBOT_LIBRARY_NAME} PRIVATE ${TGBOT_LIBRARY_PRIVATE_INC})
  list(APPEND TGBOT_LIBRARY_LIBS absl::log absl::check fmt::fmt)
  if (TGBOT_LIBRARY_STATIC)
    foreach(n ${TGBOT_LIBRARY_LIBS})
      target_include_directories(${TGBOT_LIBRARY_NAME} PRIVATE $<TARGET_PROPERTY:${n},INTERFACE_INCLUDE_DIRECTORIES>)
    endforeach()
    target_link_libraries(${TGBOT_LIBRARY_NAME} INTERFACE ${TGBOT_LIBRARY_LIBS})
  else()
    target_link_libraries(${TGBOT_LIBRARY_NAME} PUBLIC ${TGBOT_LIBRARY_LIBS})
  endif()
  target_link_libraries(${TGBOT_LIBRARY_NAME} PRIVATE ${TGBOT_LIBRARY_LIBS_STATIC})
  if (WIN32)
    target_link_libraries(${TGBOT_LIBRARY_NAME} PUBLIC ${TGBOT_LIBRARY_LIBS_WIN32})
  endif()
  target_compile_definitions(${TGBOT_LIBRARY_NAME} PUBLIC ${TGBOT_LIBRARY_DEFS} -D__TGBOT__)
  if (TGBOT_LIBRARY_DEPENDS)
    add_dependencies(${TGBOT_LIBRARY_NAME} ${TGBOT_LIBRARY_DEPENDS})
  endif()
endfunction(add_my_library)

function(add_my_executable)
  cmake_parse_arguments(TGBOT_EXE
    "NO_PREFIX;OPTIONAL" # Options
    "NAME" # One-value keywords
    "SRCS;LIBS;LIBS_WIN32;LIBS_UNIX" # Multiple-value keywords
    ${ARGN}
  )
  if (NOT TGBOT_EXE_SRCS OR NOT TGBOT_EXE_NAME)
    message(FATAL_ERROR "Must specify src or name")
  endif()
  if (DIRTY_ABSEIL_FIX)
    list(APPEND TGBOT_EXE_SRCS ${CMAKE_SOURCE_DIR}/src/logging/log_message.cc)
  endif()
  if (NOT TGBOT_EXE_NO_PREFIX)
    set(TGBOT_EXE_NAME ${PROJECT_NAME}_${TGBOT_EXE_NAME})
  endif()
  if (TGBOT_EXE_OPTIONAL)
    add_executable(${TGBOT_EXE_NAME} EXCLUDE_FROM_ALL ${TGBOT_EXE_SRCS})
  else()
    add_executable(${TGBOT_EXE_NAME} ${TGBOT_EXE_SRCS})
    install(TARGETS ${TGBOT_EXE_NAME} DESTINATION bin)
  endif()
  add_sanitizers(${TGBOT_EXE_NAME})
  target_link_libraries(${TGBOT_EXE_NAME} TgBotLogInit absl::log ${TGBOT_EXE_LIBS})
  if (WIN32)
    target_link_libraries(${TGBOT_EXE_NAME} ${TGBOT_EXE_LIBS_WIN32})
  endif()
  if (UNIX)
    target_link_libraries(${TGBOT_EXE_NAME} ${TGBOT_EXE_LIBS_UNIX})
  endif()
  target_compile_definitions(${TGBOT_EXE_NAME} PRIVATE -D__TGBOT__)
endfunction(add_my_executable)

############################ RUST support ###########################
if (ENABLE_RUST_SUPPORT)
  add_subdirectory(src/third-party/corrosion)
  corrosion_import_crate(MANIFEST_PATH src/rust/stringcalc/Cargo.toml)
endif()
#####################################################################

####################### Global compiler flags #######################
if (NOT MSVC)
  add_compile_options(
    -fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=)  
endif()
if (WIN32)
  add_compile_definitions(-DWINDOWS_BUILD)
endif()
#####################################################################

################ TgBot lib + Boost creates a warning ################
add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)
#####################################################################

######################## Include directories ########################
include_directories(src/include)
include_directories(src/)
#####################################################################

############################# Check lib #############################
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)
find_package(Protobuf)
find_package(fmt REQUIRED)
# If we couldn't find protobuf or dirty abseil fix is enabled.
if (DIRTY_ABSEIL_FIX OR NOT Protobuf_FOUND)
  # Then we can just include source-abseil
  message(STATUS "Using abseil-cpp as submodule")
  set(ABSL_PROPAGATE_CXX_STD ON)
  add_subdirectory(src/third-party/abseil-cpp)
else()
  # If there was, then we can cause collision with the system abseil
  find_package(absl REQUIRED)
endif()
#####################################################################

############################# Platforms #############################
if(WIN32)
  set(TARGET_VARIANT windows)
else()
  set(TARGET_VARIANT posix)
endif()
#####################################################################

######################## Define SQLite3 target ######################
add_subdirectory(src/database)
add_subdirectory(src/random)
add_subdirectory(src/command_modules)
add_subdirectory(src/imagep)
add_subdirectory(src/stringres)
add_subdirectory(src/logging)
add_subdirectory(src/socket)
add_subdirectory(src/utils)
add_subdirectory(src/web)
add_subdirectory(src/libos)
set(HTTPLIB_COMPILE ON)
add_subdirectory(src/third-party/cpp-httplib)
add_subdirectory(src/third-party/tgbot-cpp)
add_subdirectory(src/third-party/fruit EXCLUDE_FROM_ALL)
add_subdirectory(tests)
#################################################################

add_my_library(
  NAME PPImpl_shared_deps
  SRCS 
    src/ManagedThread.cpp
    src/ThreadManager.cpp
    src/api/MessageExt.cpp
  PUBLIC_INC
    $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>
)

################### The Bot's main functionaility ###################
add_my_library(
  NAME PPImpl
  SRCS 
    src/Authorization.cpp
    src/api/TgBotApiImpl.cpp
    src/api/CommandModule.cpp
    src/api/components/OnAnyMessage.cpp
    src/api/components/ChatJoinRequest.cpp
    src/api/components/OnCallbackQuery.cpp
    src/api/components/OnInlineQuery.cpp
    src/api/components/OnMyChatMember.cpp
    src/api/components/Async.cpp
    src/api/components/UnknownCommand.cpp
    src/global_handlers/RegEXHandler.cpp
    src/global_handlers/SpamBlocker.cpp
    src/global_handlers/ChatObserver.cpp
    src/web/TgBotWebServer.cpp
    src/logging/LoggingServer.cpp
    src/socket/interface/impl/bot/SocketDataHandler.cpp
    src/socket/interface/impl/bot/TgBotSocketInterface.cpp
    src/socket/interface/impl/backends/ServerBackend.cpp
    src/socket/interface/impl/backends/ServerBackend_${TARGET_VARIANT}.cpp
  PRIVATE_INC src/third-party/rapidjson/include
  LIBS  TgBot TgBotUtils TgBotWeb TgBotDBImpl absl::status TgBotRandom
        TgBotPPImpl_shared_deps TgBotStringRes ${CMAKE_DL_LIBS} fruit TgBotSocket
  STATIC
)
#####################################################################

################# The Bot's main launcher (program) #################
add_my_executable(
  NAME main
  SRCS src/main.cpp
  LIBS TgBotPPImpl TgBotDBLoading TgBotsighandler
  LIBS_UNIX TgBot_restartfmt_parser
)

if (UNIX)
add_my_executable(
  NAME maind
  SRCS src/mainDaemon.cpp
)
endif()
#####################################################################

install(DIRECTORY resources/ DESTINATION resources FILES_MATCHING  PATTERN "*"
  PATTERN "*.in" EXCLUDE)
