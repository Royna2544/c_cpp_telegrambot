add_subdirectory(selector)

find_package(Boost CONFIG COMPONENTS system)
add_my_library(
  NAME Socket
  SRCS
    interface/Context.cpp
    interface/Local.cpp
    interface/UDP.cpp
    interface/TCP.cpp
    interface/bot/TgBotPacketParser.cpp
    interface/bot/TgBotSocketFileHelperNew.cpp
    interface/backends/ClientBackend.cpp
    TgBotCommandMap.cpp
  PUBLIC_INC include interface
  LIBS TgBotUtils ZLIB::ZLIB OpenSSL::Crypto Boost::system
  LIBS_WIN32 wsock32 Ws2_32
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