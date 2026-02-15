tgbot_library(
  NAME
  Compiler
  SRCS
  Base.cpp
  Bash.cpp
  CCpp.cpp
  Generic.cpp
  Helper.cpp
  LIBS
  popen_wdt
  Utils
  PUBLIC_INC
  ${CMAKE_CURRENT_BINARY_DIR}
  $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>
  STATIC)

find_package(Python3 COMPONENTS Interpreter)

if (TGBOTCPP_CROSS_COMPILING)
  target_compile_definitions(Compiler INTERFACE TGBOTCPP_CROSS_COMPILING=1)
endif()

configure_file(CompilerPaths.hpp.inc
               ${CMAKE_CURRENT_BINARY_DIR}/CompilerPaths.hpp)
