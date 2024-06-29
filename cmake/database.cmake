
######################## Define SQLite3 target ######################
add_library(SQLite3 src/third-party/libsqlite3/libsqlite3/libsqlite3/sqlite3.c)
target_include_directories(SQLite3 PUBLIC src/third-party/libsqlite3/libsqlite3/libsqlite3/)
#####################################################################

####################### Database (Protobuf) #######################
include(src/database/proto/ProtobufCpp.cmake)
my_protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS src/database/proto/TgBotDB.proto)
add_library(TgBotDB SHARED 
  src/database/ProtobufDatabase.cpp
  src/database/SQLiteDatabase.cpp
  ${PROTO_SRCS})
add_dependencies(TgBotDB protobuf_TgBotDB_ready)
####################### TgBotDB lib includes #######################
get_filename_component(PROTO_HDRS_DIR ${PROTO_HDRS} DIRECTORY)
target_include_directories(TgBotDB PUBLIC ${PROTO_HDRS_DIR})
target_include_directories(TgBotDB PRIVATE ${Protobuf_INCLUDE_DIRS})
target_link_libraries(TgBotDB protobuf::libprotobuf SQLite3 TgBotUtils)
####################### TgBotDBImpl lib  #######################
add_library(TgBotDBImpl SHARED src/database/bot/TgBotDatabaseImpl.cpp)
target_link_libraries(TgBotDBImpl TgBotUtils TgBotDB)
#####################################################################

################# Utility Programs (Database Ctl) ##################
set(DBCTRL_NAME ${PROJECT_NAME}_DatabaseCtl)
add_executable_san(${DBCTRL_NAME} src/database/utils/DatabaseCtrl.cc)
target_link_libraries(${DBCTRL_NAME} TgBotDBImpl TgBotLogInit)
#####################################################################