cmake_minimum_required(VERSION 3.10.2)
project(tg_cpphost)

option(ENABLE_TIDY "Enable clang-tidy checks" OFF)
option(ENABLE_DATABASE "Enable local db support" ON)
option(ENFORCE_EN_US "Enforce en-US locale for output" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PWD_REPLACE //src_root:)
add_definitions(-DPWD_REPLACE_STR="${PWD_REPLACE}")
add_compile_options(-fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=${PWD_REPLACE})

include_directories(src/include)
include_directories(src/)

set(SRC_LIST src/main.cpp src/Authorization.cpp
             src/utils/libutils.cpp src/CompilerInTelegram.cpp
             src/BotAddCommand.cpp)

# System specific
if (WIN32)
set(SRC_LIST src/popen_wdt/popen_wdt_windows.c
             src/utils/libutils_windows.cpp
             src/windows/sighandler.c
             ${SRC_LIST})
else()
set(SRC_LIST src/popen_wdt/popen_wdt_posix.c
             src/utils/libutils_posix.cpp
             ${SRC_LIST})
endif()

if(ENABLE_TIDY)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  set(CMAKE_CXX_CLANG_TIDY
      clang-tidy;
      -header-filter=.;
      -checks=*,-llvmlibc*,-fuchsia*,-altera*,-hicpp*,abseil-*,-llvm-header-guard,-modernize-use-trailing-return-type,-modernize-redundent-void-arg
  )
endif()

if(ENABLE_DATABASE)
  add_definitions(-DUSE_DATABASE)
  find_package(Protobuf REQUIRED)
  include(cmake/ProtobufCpp.cmake)
  my_protobuf_generate_cpp(proto/gen PROTO_SRCS PROTO_HDRS proto/TgBotDB.proto)
  get_filename_component(PROTO_HDRS_DIR ${PROTO_HDRS} DIRECTORY)
  include_directories(${PROTO_HDRS_DIR})
  include_directories(${Protobuf_INCLUDE_DIRS})
  set(SRC_LIST src/Database.cpp ${SRC_LIST} ${PROTO_SRCS})
endif()

if (ENFORCE_EN_US)
  add_definitions(-DLOCALE_EN_US)
endif()

# TgBot lib + Boost warning
add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)
# C Compliance header
add_compile_options(-include sysdep/cdefs.h)

add_subdirectory(lib)
set_target_properties(TgBot PROPERTIES CXX_CLANG_TIDY "")
add_executable(${PROJECT_NAME} ${SRC_LIST})
set(LD_LIST TgBot)
if(ENABLE_DATABASE)
  set(LD_LIST ${LD_LIST} protobuf::libprotobuf)
endif()
if (WIN32)
  set(LD_LIST ${LD_LIST} wsock32 shlwapi Ws2_32)
endif()
target_link_libraries(${PROJECT_NAME} ${LD_LIST})
