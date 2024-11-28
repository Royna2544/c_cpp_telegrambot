add_my_library(
    NAME KernelBuilder-cpp
    SRCS 
        ConfigParsers2.cpp
        Compiler.cpp
        Diagnosis.cpp
        ToolchainProvider.cpp
    LIBS
        JsonCpp::JsonCpp
        BuilderUtilsLinux
    NO_LIBPREFIX
    STATIC
)

install(DIRECTORY configs/ DESTINATION share/${PROJECT_NAME}/builder/kernel)
declare_copy(
    NAME KernelBuilder
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/configs/
    DEST_DIR share/${PROJECT_NAME}/builder/kernel
    MAKE_DEPEND KernelBuilder-cpp
)