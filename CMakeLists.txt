cmake_minimum_required(VERSION 3.10.2)
project(tg_cpphost)

option(ENABLE_TIDY "Enable clang-tidy checks" OFF)
option(ENABLE_BLACKLIST "Enable blacklist by local db" ON)
option(DEBUG "Debugging extra printfs" OFF)

execute_process(
	COMMAND pwd
	RESULT_VARIABLE RESULT_PWD
	OUTPUT_VARIABLE OUTPUT_PWD
	OUTPUT_STRIP_TRAILING_WHITESPACE
)

if (RESULT_PWD EQUAL 0)
	message(STATUS "PWD is ${OUTPUT_PWD}")
	add_compile_options(-fmacro-prefix-map=${OUTPUT_PWD}=...)
endif()

set(SRC_LIST
    src/main.cpp
    src/popen_wdt/popen_wdt.c)

if (ENABLE_TIDY)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_CLANG_TIDY clang-tidy;
	-header-filter=.;
	-checks=*,-llvmlibc*,-fuchsia*,-altera*,-hicpp*,abseil-*,-llvm-header-guard,-modernize-use-trailing-return-type,-modernize-redundent-void-arg)
endif()

if (ENABLE_BLACKLIST)
add_definitions(-DUSE_BLACKLIST)
set(SRC_LIST
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
