add_my_library(
    NAME Compiler
    SRCS 
        Base.cpp
        Bash.cpp
        CCpp.cpp
        Generic.cpp
        Helper.cpp
    LIBS
        TgBotUtils
        TgBotStringResManager
        TgBot_popen_wdt
        TgBot
        absl::status
    PUBLIC_INC
        ${CMAKE_CURRENT_BINARY_DIR}
    STATIC
)

find_package(Python3 COMPONENTS Interpreter)
configure_file(CompilerPaths.hpp.inc ${CMAKE_CURRENT_BINARY_DIR}/CompilerPaths.hpp)