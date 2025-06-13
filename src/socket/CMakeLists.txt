find_package(OpenSSL REQUIRED)

find_package(Boost 1.70 CONFIG COMPONENTS system)

option(SOCKET_ENABLE_HEXDUMP
       "Enable hex-view of sent-recv packets (extremely verbose)" OFF)

set(SOCKET_HEXDUMP_LIB)
set(SOCKET_HEXDUMP_DEF)
if(SOCKET_ENABLE_HEXDUMP AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(SOCKET_HEXDUMP_LIB HexDump)
  set(SOCKET_HEXDUMP_DEF ENABLE_HEXDUMP)
endif()

tgbot_library(NAME HexDump SRCS lib/hexdump.c)

tgbot_library(
  NAME
  Socket
  SRCS
  SocketContext.cpp
  Local.cpp
  UDP.cpp
  TCP.cpp
  bot/PacketParser.cpp
  bot/FileHelperNew.cpp
  ClientBackend.cpp
  CommandMap.cpp
  PUBLIC_INC
  interface
  LIBS
  Utils
  Boost::system
  OpenSSL::Crypto
  JsonCpp::JsonCpp
  ${SOCKET_HEXDUMP_LIB}
  $<$<BOOL:${WIN32}>:wsock32
  Ws2_32>
  DEFS
  ${SOCKET_HEXDUMP_DEF})

tgbot_exe(
  NAME
  SocketCli
  SRCS
  Client.cpp
  LIBS
  Socket
  RELATION
  Socket)

include(TestBigEndian)
test_big_endian(RET)
if(${RET} EQUAL 1)
  message(FATAL_ERROR "Cannot support big-endian systems")
endif()
