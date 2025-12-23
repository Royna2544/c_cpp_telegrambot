find_package(SQLite3)
find_package(Protobuf)

if(NOT ${SQLite3_FOUND} AND NOT ${Protobuf_FOUND})
  message(FATAL_ERROR "Failed to find suitable backend, abort")
endif()
# ###################### Database (Protobuf) #######################
set(DATABASE_LIBS)
set(DATABASE_DEFS)

if(${Protobuf_FOUND})
  protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS proto/TgBotDB.proto)
  get_filename_component(PROTO_HDRS_DIR ${PROTO_HDRS} DIRECTORY)
  list(APPEND PROTO_SRCS ProtobufDatabase.cpp)
  list(APPEND DATABASE_LIBS protobuf::libprotobuf)
  list(APPEND DATABASE_DEFS DATABASE_HAVE_PROTOBUF)
else()
  set(PROTO_SRCS)
  set(PROTO_HDRS_DIR)
  file(GLOB PROTOBUF_FILELIST proto/*.proto)
  foreach(proto_file ${PROTOBUF_FILELIST})
    get_filename_component(proto_name ${proto_file} NAME_WE)
    get_filename_component(proto_path proto ABSOLUTE)
    set_source_files_properties(
      "${proto_path}/${proto_name}.pb.cc" "${proto_path}/${proto_name}.pb.h"
      PROPERTIES GENERATED TRUE)
  endforeach()
endif()

if(${SQLite3_FOUND})
  set(SQLITE_SRCS SQLiteDatabase.cpp)
  list(APPEND DATABASE_LIBS SQLite::SQLite3 cpptrace::cpptrace)
  list(APPEND DATABASE_DEFS DATABASE_HAVE_SQLITE)
endif()

tgbot_library(
  NAME
  DBImpl
  SRCS
  ${SQLITE_SRCS}
  ${PROTO_SRCS}
  bot/TgBotDatabaseImpl.cpp
  PUBLIC_INC
  ${PROTO_HDRS_DIR}
  PRIVATE_INC
  ${Protobuf_INCLUDE_DIRS}
  LIBS
  Utils
  ${DATABASE_LIBS}
  DEFS
  ${DATABASE_DEFS})

tgbot_library(
  NAME
  DBLoading
  SRCS
  bot/LoadingDatabase.cpp
  LIBS
  Utils
  DBImpl
  STATIC)

# ################ Utility Programs (Database Ctl) ##################
tgbot_exe(
  NAME
  DatabaseCtl
  SRCS
  utils/DatabaseCtrl.cc
  LIBS
  DBImpl
  DBLoading
  Utils
  RELATION
  Database)
# ##############################################################################

# ############## Utility Programs (Send Media to chat) ################
tgbot_exe(
  NAME
  MediaCli
  SRCS
  utils/SendMediaToChat.cc
  LIBS
  DBImpl
  Socket
  DBLoading
  nlohmann_json::nlohmann_json
  RELATION
  Socket)
# ##############################################################################
