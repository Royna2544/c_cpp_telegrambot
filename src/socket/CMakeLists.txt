add_subdirectory(selector)

find_package(Boost 1.70 CONFIG COMPONENTS system)
add_my_library(
  NAME Socket
  SRCS
    Context.cpp
    Local.cpp
    UDP.cpp
    TCP.cpp
    bot/PacketParser.cpp
    bot/FileHelperNew.cpp
    backends/ClientBackend.cpp
    TgBotCommandMap.cpp
  PUBLIC_INC include interface
  LIBS Utils minimalhash Boost::system
  LIBS_WIN32 wsock32 Ws2_32
)

find_package(ZLIB REQUIRED)

add_my_executable(
  NAME SocketCli
  SRCS TgBotSocketClient.cpp
  LIBS Socket
  RELATION Socket
)

include(TestBigEndian)
test_big_endian(RET)
if (${RET} EQUAL 1)
  message(FATAL_ERROR "Cannot support big-endian systems")
endif()