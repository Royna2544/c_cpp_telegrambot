add_subdirectory(selector)
add_subdirectory(interface)

add_my_library(
  NAME SocketCommandMap 
  SRCS TgBotCommandMap.cpp
  PRIVATE_INC include
)

find_package(ZLIB REQUIRED)

add_my_executable(
  NAME SocketCli
  SRCS TgBotSocketClient.cpp
  LIBS TgBotSocket OpenSSL::Crypto ZLIB::ZLIB
)

include(TestBigEndian)
test_big_endian(RET)
if (${RET} EQUAL 1)
  message(FATAL_ERROR "Cannot support big-endian systems")
endif()