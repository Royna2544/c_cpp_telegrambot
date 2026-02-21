tgbot_library(
  NAME
  Compiler
  SRCS
  Base.cpp
  Bash.cpp
  CCpp.cpp
  Generic.cpp
  Helper.cpp
  ALWAYS_STATIC)

target_link_libraries(Compiler PRIVATE popen_wdt Utils)
target_include_directories(Compiler PUBLIC $<TARGET_PROPERTY:popen_wdt,INTERFACE_INCLUDE_DIRECTORIES>)
target_include_directories(Compiler PRIVATE $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>)
find_package(Python3 COMPONENTS Interpreter)

if (TGBOTCPP_CROSS_COMPILING)
  target_compile_definitions(Compiler INTERFACE TGBOTCPP_CROSS_COMPILING=1)
endif()

configure_file(CompilerPaths.hpp.inc
               ${CMAKE_CURRENT_BINARY_DIR}/CompilerPaths.hpp)
add_library(CompilerPaths INTERFACE)
target_include_directories(CompilerPaths INTERFACE ${CMAKE_CURRENT_BINARY_DIR})