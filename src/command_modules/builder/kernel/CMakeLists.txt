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
file(COPY configs/ DESTINATION ${CMAKE_BINARY_DIR}/share/${PROJECT_NAME}/builder/kernel)
