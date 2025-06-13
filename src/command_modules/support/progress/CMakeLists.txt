add_library(progress INTERFACE)
target_include_directories(progress INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

tgbot_exe(NAME ProgressTest SRCS ProgressTest.cpp OPTIONAL NO_MAIN_WRAPPER)
