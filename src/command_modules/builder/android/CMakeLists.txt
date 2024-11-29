find_package(LibXml2 REQUIRED)
find_package(libgit2 REQUIRED)

################# Android ROM builder with C++ #################
add_my_library(
    NAME AndroidBuilder-configparse
    SRCS 
        ConfigParsers.cpp
    LIBS
        JsonCpp::JsonCpp
        LibXml2::LibXml2
        BuilderUtilsLinux
    NO_LIBPREFIX
    STATIC
)

add_my_library(
    NAME AndroidBuilder-cpp
    SRCS 
        tasks/ROMBuildTask.cpp
        tasks/UploadFileTask.cpp
        tasks/RepoSyncTask.cpp
    LIBS
        TgBot
        AndroidBuilder-configparse
        BuilderUtilsLinux
    NO_LIBPREFIX
    STATIC
)

add_my_executable(
    NAME ConfigParserTest
    SRCS ConfigParserTest.cpp
    LIBS AndroidBuilder-configparse BuilderUtilsLinux
    OPTIONAL
)

install(DIRECTORY configs/ DESTINATION share/${PROJECT_NAME}/builder/android)
file(COPY configs/ DESTINATION ${CMAKE_BINARY_DIR}/share/${PROJECT_NAME}/builder/android)
add_subdirectory(module)