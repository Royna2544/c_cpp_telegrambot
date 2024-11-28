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
declare_copy(
    NAME AndroidBuilder
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/configs/
    DEST_DIR share/${PROJECT_NAME}/builder/android
    MAKE_DEPEND AndroidBuilder-cpp
)