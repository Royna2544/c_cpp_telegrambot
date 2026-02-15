#include <string_view>

#ifndef TGBOTCPP_CROSS_COMPILING
constexpr std::string_view kCCompiler = "@CMAKE_C_COMPILER@";
constexpr std::string_view kCXXCompiler = "@CMAKE_CXX_COMPILER@";
constexpr std::string_view kPythonInterpreter = "@Python3_EXECUTABLE@";
#else
constexpr std::string_view kCCompiler = "@TGBOTCPP_CROSS_COMPILE_CC@";
constexpr std::string_view kCXXCompiler = "@TGBOTCPP_CROSS_COMPILE_CXX@";
constexpr std::string_view kPythonInterpreter = "@TGBOTCPP_CROSS_COMPILE_PYTHON@";
#endif