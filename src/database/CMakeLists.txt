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
  list(APPEND DATABASE_LIBS protobuf::libprotobuf absl::log_internal_check_op
       absl::log_internal_message)
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

# Optional: cpptrace for backtraces
find_package(cpptrace)
set(cpptrace_def)
set(cpptrace_libs)
if (cpptrace_FOUND)
  message(STATUS "Enabling cpptrace for backtraces in TgBotApiImpl")
  set(cpptrace_def TGBOTCPP_ENABLE_CPPTRACE)
  set(cpptrace_libs cpptrace::cpptrace)
endif()

if(SQLite3_FOUND)
  set(SQLITE_SRCS SQLiteDatabase.cpp)
  list(APPEND DATABASE_LIBS SQLite::SQLite3 ${cpptrace_libs})
  list(APPEND DATABASE_DEFS DATABASE_HAVE_SQLITE ${cpptrace_def})
endif()

tgbot_library(
  NAME DBImpl
  SRCS
    ${SQLITE_SRCS}
    ${PROTO_SRCS}
    bot/TgBotDatabaseImpl.cpp
)
target_include_directories(DBImpl PUBLIC ${PROTO_HDRS_DIR})
target_include_directories(DBImpl PRIVATE ${Protobuf_INCLUDE_DIRS})
target_link_libraries(DBImpl PRIVATE ${DATABASE_LIBS})
target_link_libraries(DBImpl PUBLIC Utils)
target_compile_definitions(DBImpl PUBLIC ${DATABASE_DEFS})

tgbot_library(
  NAME DBLoading
  SRCS
    bot/LoadingDatabase.cpp
  ALWAYS_STATIC
)
target_link_libraries(DBLoading PRIVATE Utils DBImpl)

# ################ Utility Programs (Database Ctl) ##################
tgbot_exe(
  NAME DatabaseCtl
  SRCS
    utils/DatabaseCtrl.cc
  RELATION Database
)
target_link_libraries(DatabaseCtl PRIVATE DBImpl DBLoading Utils)
# ##############################################################################

# ############## Utility Programs (Send Media to chat) ################
tgbot_exe(
  NAME MediaCli
  SRCS
     utils/SendMediaToChat.cc
  RELATION Socket
)
target_link_libraries(MediaCli PRIVATE DBImpl SocketServiceProto DBLoading nlohmann_json::nlohmann_json)
# ##############################################################################
