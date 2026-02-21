
include(gRPCGenerate)
grpc_generate(Socket)

tgbot_library(
    NAME SocketService
    SRCS SocketServiceImpl.cpp
    ALWAYS_STATIC
)
target_link_libraries(SocketService PRIVATE stduuid cryptopp SocketServiceProto)
target_include_directories(SocketService PUBLIC $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>)

tgbot_exe(
    NAME SocketClient
    SRCS SocketServiceClient.cpp
    RELATION Socket
)
target_link_libraries(SocketClient PRIVATE SocketServiceProto)