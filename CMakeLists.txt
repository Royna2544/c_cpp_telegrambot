cmake_minimum_required(VERSION 3.10.2)
project(tg_cpphost)

set(SRC_LIST
    src/main.cpp)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_CLANG_TIDY clang-tidy;
	-header-filter=.;
	-checks=*,-llvmlibc*,-fuchsia*,-altera*,-hicpp*,abseil-*,-llvm-header-guard,-modernize-use-trailing-return-type,-modernize-redundent-void-arg)

add_subdirectory(lib)
add_executable(${PROJECT_NAME} ${SRC_LIST})
target_link_libraries(${PROJECT_NAME} TgBot)
