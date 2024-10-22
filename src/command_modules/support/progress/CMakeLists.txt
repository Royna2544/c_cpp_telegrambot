add_library(progress INTERFACE)
target_include_directories(progress INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

add_my_executable(
    NAME ProgressTest
    SRCS ProgressTest.cpp
    OPTIONAL
)