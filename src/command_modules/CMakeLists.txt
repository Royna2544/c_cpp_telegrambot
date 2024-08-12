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
            if (APPLE OR WIN32)
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
    set(TGBOT_CMDMODULE_LIBNAME cmd_${TGBOT_CMDMODULE_NAME})
    add_library(${TGBOT_CMDMODULE_LIBNAME} ${TGBOT_CMDMODULE_SRCS})
    target_link_libraries(${TGBOT_CMDMODULE_LIBNAME} ${TGBOT_CMDMODULE_LIBS}
                          TgBotPPImpl_shared_deps absl::log TgBotStringResManager)
    install(TARGETS ${TGBOT_CMDMODULE_LIBNAME} DESTINATION lib/commands)
    add_sanitizers(${TGBOT_CMDMODULE_LIBNAME})
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
    LIBS absl::strings TgBotDBImpl
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
    NAME ibash
    SRCS ibash.cpp
    LIBS TgBot_portable_semaphore TgBot_popen_wdt TgBotSocketSelectors
    PLATFORM "Unix"
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

command_module(
    NAME restart
    SRCS restart.cpp
    LIBS TgBotCommandLine TgBot_restartfmt_parser
    PLATFORM "Unix"
)

command_module(
    NAME rombuild
    SRCS romBuild.cpp
        androidbuilder_tasks/ROMBuildTask.cpp
        androidbuilder_tasks/UploadFileTask.cpp
    LIBS AndroidBuilder-cpp
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

multi_command_module(
    PROVIDES starttimer stoptimer
    SRCS timer_impl.cpp support/timer/TimerImpl.cpp
)

command_module(
    NAME copystickers
    SRCS copystickers.cpp
    LIBS TgBotImgProc
)
