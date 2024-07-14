
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
set(DBCTRL_NAME ${PROJECT_NAME}_DatabaseCtl)
add_executable_san(${DBCTRL_NAME} utils/DatabaseCtrl.cc)
target_link_libraries(${DBCTRL_NAME} TgBotDBImpl TgBotLogInit)
#####################################################################

############### Utility Programs (Send Media to chat) ################
if (USE_UNIX_SOCKETS)
  set(MEDIA_CLI_NAME ${PROJECT_NAME}_MediaCli)
  add_executable_san(${MEDIA_CLI_NAME} utils/SendMediaToChat.cc)
  target_link_libraries(${MEDIA_CLI_NAME} TgBotDBImpl TgBotSocket TgBotLogInit)
  target_link_lib_if_windows(${MEDIA_CLI_NAME} Ws2_32)
endif()
#####################################################################