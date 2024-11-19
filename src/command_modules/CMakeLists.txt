set(CMDMODULE_PREFIX cmd_)

function(command_module)
    cmake_parse_arguments(TGBOT_CMDMODULE
        "RUST" # Options
        "NAME;PLATFORM;PACKAGE" # One-value keywords
        "SRCS;LIBS" # Multiple-value keywords
        ${ARGN}
    )
    if (NOT (TGBOT_CMDMODULE_SRCS AND TGBOT_CMDMODULE_NAME))
        message(FATAL_ERROR "SRCS and NAME must be specified")
    endif()
    if (TGBOT_CMDMODULE_RUST AND NOT ENABLE_RUST_SUPPORT)
        message(STATUS "Command ${TGBOT_CMDMODULE_NAME} is disabled (No Rust support)")
        return()
    endif()
    if (TGBOT_CMDMODULE_PLATFORM)
        # TODO: Adding more platforms
        if (TGBOT_CMDMODULE_PLATFORM STREQUAL "Unix")
            if (NOT UNIX)
               message(STATUS "Command ${TGBOT_CMDMODULE_NAME} is disabled (Not Unix)")
               return()
            endif()
        elseif(TGBOT_CMDMODULE_PLATFORM STREQUAL "Linux")
            if (NOT LINUX)
                message(STATUS "Command ${TGBOT_CMDMODULE_NAME} is disabled (Not Linux)")
                return()
            endif()
        else()
            message(WARNING "Unknown platform: ${TGBOT_CMDMODULE_PLATFORM}")
        endif()
    endif()
    if (TGBOT_CMDMODULE_PACKAGE)
        find_package (${TGBOT_CMDMODULE_PACKAGE})
        if (NOT ${TGBOT_CMDMODULE_PACKAGE}_FOUND)
            message(WARNING "Package not found, disable command ${TGBOT_CMDMODULE_NAME}")
        endif()
    endif()
    set(TGBOT_CMDMODULE_LIBNAME ${CMDMODULE_PREFIX}${TGBOT_CMDMODULE_NAME})
    add_library(${TGBOT_CMDMODULE_LIBNAME} SHARED ${TGBOT_CMDMODULE_SRCS})
    set_target_properties(${TGBOT_CMDMODULE_LIBNAME} PROPERTIES PREFIX "lib")
    target_link_libraries(${TGBOT_CMDMODULE_LIBNAME} ${TGBOT_CMDMODULE_LIBS} absl::log)
    target_include_directories(${TGBOT_CMDMODULE_LIBNAME} PRIVATE
        $<TARGET_PROPERTY:TgBotStringRes,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:TgBotRandom,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>)
    add_sanitizers(${TGBOT_CMDMODULE_LIBNAME})
    # Install command modules to lib/modules
    install(TARGETS ${TGBOT_CMDMODULE_LIBNAME} DESTINATION lib/modules)
    set_target_properties(${TGBOT_CMDMODULE_LIBNAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/modules)
    # PIE
    if (PIE_SUPPORTED)
      set_target_properties(${TGBOT_CMDMODULE_LIBNAME} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()
    # Global variables
    target_compile_definitions(${TGBOT_CMDMODULE_LIBNAME} PRIVATE ${GLOBAL_DEFINITIONS})
    target_compile_options(${TGBOT_CMDMODULE_LIBNAME} PRIVATE ${GLOBAL_COMPILE_OPTIONS})
endfunction()

function(multi_command_module)
    cmake_parse_arguments(TGBOT_MULTI_CMDMODULE
        "" # Options
        "PLATFORM" # One-value keywords
        "SRCS;LIBS;PROVIDES" # Multiple-value keywords
        ${ARGN}
    )
    if (NOT TGBOT_MULTI_CMDMODULE_PROVIDES)
        message(FATAL_ERROR "Must specify modules provided by this source")
    endif()
    foreach (module ${TGBOT_MULTI_CMDMODULE_PROVIDES})
        # Forward the arguments
        command_module(
            NAME ${module}
            SRCS ${TGBOT_MULTI_CMDMODULE_SRCS}
            LIBS ${TGBOT_MULTI_CMDMODULE_LIBS}
            PLATFORM ${TGBOT_MULTI_CMDMODULE_PLATFORM}
        )
    endforeach()
endfunction()

add_subdirectory(support)
add_subdirectory(compiler)

multi_command_module(
    PROVIDES alive start
    SRCS alive.cpp
    LIBS absl::strings TgBotDBImpl TgBotUtils
)

multi_command_module(
    PROVIDES bash ubash
    SRCS bash_impl.cpp
    LIBS TgBotCompiler
)

multi_command_module(
    PROVIDES c cpp
    SRCS compilers_impl.cpp
    LIBS TgBotCompiler
)

command_module(
    NAME py
    SRCS compilers_impl.cpp
    LIBS TgBotCompiler
    PACKAGE Python3
)

command_module(
    NAME cmd
    SRCS cmd.cpp
)

multi_command_module(
    PROVIDES database saveid
    SRCS database_impl.cpp
    LIBS TgBotDBImpl TgBotRandom
)

command_module(
    NAME decho
    SRCS decho.cpp
)

command_module(
    NAME decide
    SRCS decide.cpp
    LIBS TgBotRandom
)

command_module(
    NAME delay
    SRCS delay.cpp
)

command_module(
    NAME fileid
    SRCS fileid.cpp
)

command_module(
    NAME flash
    SRCS flash.cpp
    LIBS TgBotRandom
)

command_module(
    NAME possibility
    SRCS possibility.cpp
    LIBS TgBotRandom
)

command_module(
    NAME randsticker
    SRCS randsticker.cpp
    LIBS TgBotRandom
)

add_subdirectory(android_builder)
command_module(
    NAME rombuild
    SRCS romBuild.cpp
        android_builder/tasks/ROMBuildTask.cpp
        android_builder/tasks/UploadFileTask.cpp
        android_builder/tasks/RepoSyncTask.cpp
    LIBS AndroidBuilder-cpp TgBotsighandler
    PLATFORM "Linux"
)

command_module(
    NAME rotatepic
    SRCS rotatepic.cpp
    LIBS TgBotImgProc
)

command_module(
    NAME setowner
    SRCS setowner.cpp
    LIBS TgBotDBImpl
)

command_module(
    NAME spam
    SRCS spam.cpp
)

command_module(
    NAME stringcalc
    SRCS stringCalc.cpp
    LIBS stringcalc
    RUST
)

command_module(
    NAME copystickers
    SRCS copystickers.cpp
    LIBS TgBotImgProc TgBotUtils
)
