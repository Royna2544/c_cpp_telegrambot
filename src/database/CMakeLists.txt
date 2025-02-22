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
  list(APPEND DATABASE_DEFS DATABASE_HAVE_PROTOBUF)
else()
  set(PROTO_SRCS)
  set(PROTO_HDRS_DIR)
  file(GLOB PROTOBUF_FILELIST proto/*.proto)
  foreach(proto_file ${PROTOBUF_FILELIST})
   get_filename_component(proto_name ${proto_file} NAME_WE)
   get_filename_component(proto_path proto ABSOLUTE)
   set_source_files_properties("${proto_path}/${proto_name}.pb.cc"
                               "${proto_path}/${proto_name}.pb.h"
                               PROPERTIES GENERATED TRUE)
  endforeach()
endif()

if (${SQLite3_FOUND})
  set(SQLITE_SRCS SQLiteDatabase.cpp)
  list(APPEND DATABASE_LIBS SQLite::SQLite3)
  list(APPEND DATABASE_DEFS DATABASE_HAVE_SQLITE)
endif()

add_my_library(
  NAME DBImpl
  SRCS
    ${SQLITE_SRCS}
    ${PROTO_SRCS}
    bot/TgBotDatabaseImpl.cpp
  PUBLIC_INC 
    ${PROTO_HDRS_DIR}
  PRIVATE_INC
    ${Protobuf_INCLUDE_DIRS}
  LIBS_STATIC
    ${DATABASE_LIBS}
  LIBS Utils
  DEFS
    ${DATABASE_DEFS}
)

add_my_library(
  NAME DBLoading
  SRCS bot/LoadingDatabase.cpp
  LIBS Utils DBImpl
  STATIC
)

################# Utility Programs (Database Ctl) ##################
add_my_executable(
  NAME DatabaseCtl
  SRCS utils/DatabaseCtrl.cc
  LIBS DBImpl DBLoading Utils
  RELATION Database
)
#####################################################################

############### Utility Programs (Send Media to chat) ################
add_my_executable(
  NAME MediaCli
  SRCS utils/SendMediaToChat.cc
  LIBS 
    DBImpl
    Socket
    DBLoading
    JsonCpp::JsonCpp
  RELATION Socket
)
#####################################################################
