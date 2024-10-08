find_package(SQLite3)
find_package(Protobuf)

if (NOT ${SQLite3_FOUND} AND NOT ${Protobuf_FOUND})
  message(FATAL_ERROR "Failed to find suitable backend, abort")
endif()
####################### Database (Protobuf) #######################
set(DATABASE_LIBS)
set(DATABASE_DEFS)
if (${Protobuf_FOUND})
  protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS proto/TgBotDB.proto)
  get_filename_component(PROTO_HDRS_DIR ${PROTO_HDRS} DIRECTORY)
  list(APPEND PROTO_SRCS ProtobufDatabase.cpp)
  list(APPEND DATABASE_LIBS protobuf::libprotobuf absl::log_internal_check_op absl::log_internal_message)
  list(APPEND DATABASE_DEFS -DHAVE_PROTOBUF)
else()
  set(PROTO_SRCS)
  set(PROTO_HDRS_DIR)
endif()

if (${SQLite3_FOUND})
  set(SQLITE_SRCS SQLiteDatabase.cpp)
  list(APPEND DATABASE_LIBS SQLite::SQLite3)
  list(APPEND DATABASE_DEFS -DHAVE_SQLITE)
endif()

add_my_library(
  NAME DB 
  SRCS
    ${SQLITE_SRCS}
    ${PROTO_SRCS}
  PUBLIC_INC 
    ${PROTO_HDRS_DIR}
  PRIVATE_INC
    ${Protobuf_INCLUDE_DIRS}
  LIBS
    ${DATABASE_LIBS} TgBotUtils
)

add_my_library(
  NAME DBImpl
  SRCS bot/TgBotDatabaseImpl.cpp
  LIBS TgBotDB
  DEFS
    ${DATABASE_DEFS}
)

################# Utility Programs (Database Ctl) ##################
add_my_executable(
  NAME DatabaseCtl
  SRCS utils/DatabaseCtrl.cc
  LIBS TgBotDBImpl
)
#####################################################################

############### Utility Programs (Send Media to chat) ################
if (USE_UNIX_SOCKETS)
  add_my_executable(
    NAME MediaCli
    SRCS utils/SendMediaToChat.cc
    LIBS 
      TgBotDBImpl
      TgBotSocket
      OpenSSL::Crypto
  )
endif()
#####################################################################