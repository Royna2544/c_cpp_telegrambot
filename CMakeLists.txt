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
             src/CompilerInTelegram.cpp src/BotAddCommand.cpp)

set(LIBUTILS_SRC_LIST src/utils/libutils.cpp)

# System specific
if (WIN32)
set(SRC_LIST src/windows/sighandler.c
	     ${SRC_LIST})

set(LIBUTILS_SRC_LIST src/popen_wdt/popen_wdt_windows.c
             src/utils/libutils_windows.cpp
     	     ${LIBUTILS_SRC_LIST})
else()
set(LIBUTILS_SRC_LIST src/popen_wdt/popen_wdt_posix.c
             src/utils/libutils_posix.cpp
             ${LIBUTILS_SRC_LIST})
endif()

if(ENABLE_TIDY)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
  set(CMAKE_CXX_CLANG_TIDY
      clang-tidy;
      -header-filter=.;
      -checks=*,-llvmlibc*,-fuchsia*,-altera*,-hicpp*,abseil-*,-llvm-header-guard,-modernize-use-trailing-return-type,-modernize-redundent-void-arg
  )
  set_target_properties(TgBot PROPERTIES CXX_CLANG_TIDY "")
endif()

if(ENABLE_DATABASE)
  add_definitions(-DUSE_DATABASE)
  find_package(Protobuf REQUIRED)
  include(cmake/ProtobufCpp.cmake)
  my_protobuf_generate_cpp(proto/gen PROTO_SRCS PROTO_HDRS proto/TgBotDB.proto)
  get_filename_component(PROTO_HDRS_DIR ${PROTO_HDRS} DIRECTORY)
  include_directories(${PROTO_HDRS_DIR})
  include_directories(${Protobuf_INCLUDE_DIRS})
  add_library(TgBotDB STATIC src/Database.cpp ${PROTO_SRCS})
  target_link_libraries(TgBotDB TgBot TgBotUtils protobuf::libprotobuf)
endif()

if (ENFORCE_EN_US)
  add_definitions(-DLOCALE_EN_US)
endif()

# TgBot lib + Boost warning
add_definitions(-DBOOST_BIND_GLOBAL_PLACEHOLDERS)

# Utils library
add_library(TgBotUtils STATIC ${LIBUTILS_SRC_LIST})
# Windows needs this
if (WIN32)
target_link_libraries(TgBotUtils shlwapi)
endif()

# TgBot-Cpp library
add_subdirectory(lib)
# Build main tg_cpphost exe
add_executable(${PROJECT_NAME} ${SRC_LIST})

# Default LD-list
set(LD_LIST TgBot TgBotUtils)

if(ENABLE_DATABASE)
  set(LD_LIST ${LD_LIST} absl_log_internal_check_op absl_log_internal_message)
  set(LD_LIST ${LD_LIST} TgBotDB)
endif()
if (WIN32)
  set(LD_LIST ${LD_LIST} wsock32 Ws2_32)
endif()
target_link_libraries(${PROJECT_NAME} ${LD_LIST})

## Protobuf DB Dumper
if (ENABLE_DATABASE)
add_executable(dumpProtoDB proto/DumpProtoDB.cc)
target_link_libraries(dumpProtoDB ${LD_LIST})
endif()
