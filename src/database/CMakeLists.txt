
####################### Database (Protobuf) #######################
include(proto/ProtobufCpp.cmake)
my_protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS proto/TgBotDB.proto)
####################### TgBotDB lib includes #######################
get_filename_component(PROTO_HDRS_DIR ${PROTO_HDRS} DIRECTORY)
add_my_library(
  NAME DB 
  SRCS
    ProtobufDatabase.cpp
    SQLiteDatabase.cpp
    ${PROTO_SRCS}
  DEPENDS
    protobuf_TgBotDB_ready
  PUBLIC_INC 
    ${PROTO_HDRS_DIR}
  PRIVATE_INC
    ${Protobuf_INCLUDE_DIRS}
  LIBS
    protobuf::libprotobuf SQLite3 TgBotUtils
)

add_my_library(
  NAME DBImpl
  SRCS bot/TgBotDatabaseImpl.cpp
  LIBS TgBotUtils TgBotDB
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
  )
endif()
#####################################################################