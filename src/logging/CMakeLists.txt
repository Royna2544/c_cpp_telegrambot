include(gRPCGenerate)
grpc_generate(Log)

tgbot_exe(
  NAME logcat
  SRCS
    LoggingClient.cpp
  RELATION
  Socket
)

target_link_libraries(logcat PRIVATE LogServiceProto Utils sighandler)
