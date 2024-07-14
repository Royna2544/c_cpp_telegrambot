find_package(Python COMPONENTS Development REQUIRED)

add_my_library(
    NAME AndroidBuilder-cpp
    SRCS 
        PythonClass.cpp
        ArgumentBuilder.cpp
        RepoUtils.cpp
        ForkAndRun.cpp
        ConfigParsers.cpp
        tasks/RepoSyncTask.cpp
    LIBS
        ${Python_LIBRARIES}
        TgBotLogInit
        TgBotRandom
        TgBotSocketSelectors
        ${LIBGIT2_LIBS}
    PUBLIC_INC
        .
        ${Python_INCLUDE_DIRS}
    NO_LIBSUFFIX
)
