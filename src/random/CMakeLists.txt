tgbot_library(NAME Random SRCS Random.cpp)

# ################ Detecting x86/x86_64 Architecture ################
if(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86|x86_64|AMD64|amd64" AND NOT MSVC)
  target_compile_options(Random PRIVATE -mrdrnd)
endif()

target_include_directories(Random INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})