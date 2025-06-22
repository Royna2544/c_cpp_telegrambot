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

# Using flatbuffers
set(CURRENT_VERSION 1)
find_package(FlatBuffers CONFIG REQUIRED)

# Generate FlatBuffers binding
set(SCHEMAS
    CtrlSpamBlock
    GenericAck
    GetUptimeCallback
    ObserveChat
    PacketHeader
    SendFileToChat
    TransferFileBegin
    TransferFileChunk
    TransferFileChunkResponse
    TransferFileEnd
    TransferFileRequest
    WriteMessageToChat
)

set(SCHEMA_PROTOCOL Protocol_V${CURRENT_VERSION})
add_custom_target(${SCHEMA_PROTOCOL}_all)

set(SCHEMA_NAMES)
include(flatc)
foreach(SCHEMA ${SCHEMAS})
    set(_tgt ${SCHEMA_PROTOCOL}_${SCHEMA})
    list(APPEND SCHEMA_NAMES ${_tgt})
    add_flatbuffer_schema(
        TARGET ${_tgt}
	SCHEMA schemas/${CURRENT_VERSION}/${SCHEMA}.fbs
	LANGS cpp
	OUT_DIR ${CMAKE_BINARY_DIR}/flatc_gen/flatbuffers
    )
    add_dependencies(${SCHEMA_PROTOCOL}_all ${_tgt})
endforeach()

# HexDump support
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
  flatbuffers::flatbuffers
  ${SOCKET_HEXDUMP_LIB}
  $<$<BOOL:${WIN32}>:wsock32 Ws2_32>
  DEPENDS
  ${SCHEMA_PROTOCOL}_all
  PRIVATE_INC
  ${CMAKE_BINARY_DIR}/flatc_gen
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
