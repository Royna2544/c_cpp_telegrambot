find_package(OpenSSL REQUIRED)

find_package(Boost 1.70 CONFIG COMPONENTS system)

option(TGBOTCPP_SOCKET_PACKET_VERBOSE "TgBot-Cpp: Enable verbose hex-view level logging for socket packets" OFF)

set(SOCKET_HEXDUMP_LIB)
set(SOCKET_HEXDUMP_DEF)
if(TGBOTCPP_SOCKET_PACKET_VERBOSE AND CMAKE_BUILD_TYPE STREQUAL "Debug")
  set(SOCKET_HEXDUMP_LIB HexDump)
  set(SOCKET_HEXDUMP_DEF ENABLE_HEXDUMP)
endif()

tgbot_library(NAME HexDump SRCS lib/hexdump.c STATIC)

tgbot_library(
  NAME
  Socket
  SRCS
  SocketContext.cpp
  context/Local.cpp
  context/UDP.cpp
  context/TCP.cpp
  bot/PacketParser.cpp
  bot/FileHelperNew.cpp
  ClientBackend.cpp
  CommandMap.cpp
  LIBS
  Utils
  Boost::system
  OpenSSL::Crypto
  nlohmann_json::nlohmann_json
  ${SOCKET_HEXDUMP_LIB}
  $<$<BOOL:${WIN32}>:wsock32
  Ws2_32>
  DEFS
  ${SOCKET_HEXDUMP_DEF})

tgbot_exe(
  NAME
  SocketCli
  SRCS
  client/CallbackHandler.cpp
  client/CommandParser.cpp
  client/PacketBuilder.cpp
  client/SessionManager.cpp
  client/main.cpp
  LIBS
  Socket
  RELATION
  Socket)

  # Add API headers as interface library
add_library(socket_api INTERFACE)

target_include_directories(socket_api INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(socket_api INTERFACE
  FILE_SET HEADERS
  BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
  FILES
  ApiDef.hpp
  api/CoreTypes.hpp
  api/ByteHelper.hpp
  api/Utilities.hpp
  api/Commands.hpp
  api/Packet.hpp
  api/DataStructures.hpp
  api/Callbacks.hpp
  api/SizeAssertions.hpp
)

# Existing socket library now depends on socket_api
target_link_libraries(Socket PUBLIC socket_api)
