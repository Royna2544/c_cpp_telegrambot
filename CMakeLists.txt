cmake_minimum_required(VERSION 3.10.2)
project(tg_cpphost)

option(ENABLE_TIDY "Enable clang-tidy checks" OFF)
option(ENABLE_DATABASE "Enable local db support" ON)
option(DEBUG "Debugging extra printfs" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
	message(FATAL_ERROR "Windows not supported")
endif()

execute_process(
	COMMAND pwd
	RESULT_VARIABLE RESULT_PWD
	OUTPUT_VARIABLE OUTPUT_PWD
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
        COMMAND git rev-parse HEAD
        RESULT_VARIABLE RESULT_GIT_COMMITID
        OUTPUT_VARIABLE OUTPUT_GIT_COMMITID
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
        COMMAND bash -c "git log -1 --pretty=%B | head -n 1"
        RESULT_VARIABLE RESULT_GIT_COMMITMSG
        OUTPUT_VARIABLE OUTPUT_GIT_COMMITMSG
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
        COMMAND git config --get remote.origin.url
        RESULT_VARIABLE RESULT_GIT_ORIGIN_URL
        OUTPUT_VARIABLE OUTPUT_GIT_ORIGIN_URL
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

if (RESULT_PWD EQUAL 0)
	message(STATUS "PWD is ${OUTPUT_PWD}")
	add_compile_options(-fmacro-prefix-map=${OUTPUT_PWD}=...)
endif()

if (RESULT_GIT_COMMITID EQUAL 0)
        message(STATUS "Commit ID available")
        add_definitions(-DGIT_COMMITID="${OUTPUT_GIT_COMMITID}")
endif()

if (RESULT_GIT_COMMITMSG EQUAL 0)
        message(STATUS "Commit msg available")
        add_definitions(-DGIT_COMMITMSG="${OUTPUT_GIT_COMMITMSG}")
endif()

if (RESULT_GIT_ORIGIN_URL EQUAL 0)
        message(STATUS "origin url link available")
	add_definitions(-DGIT_ORIGIN_URL="${OUTPUT_GIT_ORIGIN_URL}")
endif()

include_directories(src/include)
include_directories(src/)

set(SRC_LIST
    src/main.cpp
    src/Authorization.cpp
    src/popen_wdt/popen_wdt.c)

if (ENABLE_TIDY)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_CLANG_TIDY clang-tidy;
	-header-filter=.;
	-checks=*,-llvmlibc*,-fuchsia*,-altera*,-hicpp*,abseil-*,-llvm-header-guard,-modernize-use-trailing-return-type,-modernize-redundent-void-arg)
endif()

if (ENABLE_DATABASE)
add_definitions(-DUSE_DATABASE)
set(SRC_LIST
    src/Database.cpp
    src/conf/conf.cc
    ${SRC_LIST})
endif()

if (DEBUG)
add_definitions(-DDEBUG)
endif()

add_subdirectory(lib)
set_target_properties(TgBot
    PROPERTIES CXX_CLANG_TIDY ""
)
add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME} TgBot)
