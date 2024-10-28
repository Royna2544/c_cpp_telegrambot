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
    target_link_libraries(${TGBOT_CMDMODULE_LIBNAME} ${TGBOT_CMDMODULE_LIBS}
                          TgBotPPImpl_shared_deps absl::log)
    target_include_directories(${TGBOT_CMDMODULE_LIBNAME} PRIVATE
        $<TARGET_PROPERTY:TgBotStringRes,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:TgBotRandom,INTERFACE_INCLUDE_DIRECTORIES>)
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
    if (UNIX)
        list(GET TGBOT_MULTI_CMDMODULE_PROVIDES 0 PROVIDES_FIRST) # Forward the arguments
        command_module(
            NAME ${PROVIDES_FIRST}
            SRCS ${TGBOT_MULTI_CMDMODULE_SRCS}
            LIBS ${TGBOT_MULTI_CMDMODULE_LIBS}
            PLATFORM ${TGBOT_MULTI_CMDMODULE_PLATFORM}
        )
        list(REMOVE_AT TGBOT_MULTI_CMDMODULE_PROVIDES 0)
        foreach (module ${TGBOT_MULTI_CMDMODULE_PROVIDES})
            set(SOURCE_FILENAME ${CMAKE_SHARED_LIBRARY_PREFIX}${CMDMODULE_PREFIX}${PROVIDES_FIRST}${CMAKE_SHARED_LIBRARY_SUFFIX})
            set(DEST_FILENAME ${CMAKE_SHARED_LIBRARY_PREFIX}${CMDMODULE_PREFIX}${module}${CMAKE_SHARED_LIBRARY_SUFFIX})
            add_custom_target(
                ${CMDMODULE_PREFIX}${module} ALL
                COMMAND cmake -E create_symlink ${SOURCE_FILENAME} ${DEST_FILENAME}
                WORKING_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}
                DEPENDS cmd_${PROVIDES_FIRST}
                COMMENT "Creating library symlink ${SOURCE_FILENAME} => ${DEST_FILENAME}"
            )
        endforeach()
    else()
        foreach (module ${TGBOT_MULTI_CMDMODULE_PROVIDES})
            # Forward the arguments
            command_module(
                NAME ${module}
                SRCS ${TGBOT_MULTI_CMDMODULE_SRCS}
                LIBS ${TGBOT_MULTI_CMDMODULE_LIBS}
                PLATFORM ${TGBOT_MULTI_CMDMODULE_PLATFORM}
            )
        endforeach()
    endif()
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

command_module(
    NAME restart
    SRCS restart.cpp
    LIBS TgBotCommandLine TgBot_restartfmt_parser
    PLATFORM "Unix"
)

add_subdirectory(android_builder)
command_module(
    NAME rombuild
    SRCS romBuild.cpp
        android_builder/tasks/ROMBuildTask.cpp
        android_builder/tasks/UploadFileTask.cpp
        android_builder/tasks/RepoSyncTask.cpp
    LIBS AndroidBuilder-cpp TgBot
    PLATFORM "Linux"
)

command_module(
    NAME rotatepic
    SRCS rotatepic.cpp
    LIBS TgBotImgProc TgBot
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
    LIBS TgBotImgProc TgBot
)
