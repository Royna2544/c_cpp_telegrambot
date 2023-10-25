cmake_minimum_required(VERSION 3.10.2)
project(tg_cpphost)

option(ENABLE_TIDY "Enable clang-tidy checks" OFF)
option(ENABLE_DATABASE "Enable local db support" ON)
option(ENFORCE_EN_US "Enforce en-US locale for output" ON)
if (NOT WIN32)
  option(ENABLE_RUNTIME_COMMAND "Enable runtime loader of commands" ON)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(PWD_REPLACE //${PROJECT_NAME}:)
add_definitions(-DPWD_REPLACE_STR="${PWD_REPLACE}")
add_compile_options(-fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/=${PWD_REPLACE})

include_directories(src/include)
include_directories(src/)

if (WIN32)
  set(TARGET_VARIANT windows)
else()
  set(TARGET_VARIANT posix)
endif()

set(SRC_LIST src/main.cpp src/Authorization.cpp
             src/CompilerInTelegram.cpp src/BotAddCommand.cpp
             src/exithandlers/handler_${TARGET_VARIANT}.cpp)

set(LIBUTILS_SRC_LIST src/utils/libutils.cpp
             src/popen_wdt/popen_wdt_${TARGET_VARIANT}.c
	     src/utils/libutils_${TARGET_VARIANT}.cpp)

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

if (ENABLE_RUNTIME_COMMAND)
  add_definitions(-DRTCOMMAND_LOADER)
  set(SRC_LIST src/RTCommandLoader.cpp
	       ${SRC_LIST})
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
if (ENABLE_RUNTIME_COMMAND)
  # Dynamic cmd loader
  add_subdirectory(src/cmd_dynamic)
endif()
# Build main tg_cpphost exe
add_executable(${PROJECT_NAME} ${SRC_LIST})

# Default LD-list
set(LD_LIST TgBot TgBotUtils)

if(ENABLE_DATABASE)
  foreach(ABSL_LIB absl_log_internal_check_op absl_log_internal_message)
      find_library(TMP_${ABSL_LIB} NAMES ${ABSL_LIB})
      if (NOT TMP_${ABSL_LIB})
	  message(INFO " Library ${ABSL_LIB} not existent")
	  continue()
      endif()
      set(LD_LIST ${LD_LIST} ${TMP_${ABSL_LIB}})
  endforeach()
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
