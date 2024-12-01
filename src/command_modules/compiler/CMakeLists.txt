add_my_library(
    NAME Compiler
    SRCS 
        Base.cpp
        Bash.cpp
        CCpp.cpp
        Generic.cpp
        Helper.cpp
    LIBS
        popen_wdt
        StringRes
        absl::status
        Utils
    PUBLIC_INC
        ${CMAKE_CURRENT_BINARY_DIR}
        $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>
    STATIC
)

find_package(Python3 COMPONENTS Interpreter)
configure_file(CompilerPaths.hpp.inc ${CMAKE_CURRENT_BINARY_DIR}/CompilerPaths.hpp)