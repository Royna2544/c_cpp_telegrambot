find_package(Python COMPONENTS Development REQUIRED)

add_my_library(
    NAME Python
    SRCS
        PythonClass.cpp
        ArgumentBuilder.cpp
    LIBS
        ${Python_LIBRARIES}
    PUBLIC_INC
        ${Python_INCLUDE_DIRS}
        .
)

################# Android ROM builder with C++/Python #################
if (UNIX AND NOT APPLE)
add_my_library(
    NAME AndroidBuilder-cpp
    SRCS 
        RepoUtils.cpp
        ForkAndRun.cpp
        ConfigParsers.cpp
        tasks/RepoSyncTask.cpp
    LIBS
        TgBotLogInit
        TgBotRandom
        TgBotSocketSelectors
        TgBotPython
        ${LIBGIT2_LIBS}
    PUBLIC_INC
        .
    NO_LIBPREFIX
)
endif()
