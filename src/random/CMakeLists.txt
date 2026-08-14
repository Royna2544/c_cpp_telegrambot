tgbot_library(NAME Random SRCS Random.cpp)

target_include_directories(Random INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
  target_link_libraries(Random PRIVATE bcrypt)
endif()
