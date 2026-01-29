
include(gRPCGenerate)
grpc_generate(Socket)

tgbot_library(
    NAME SocketService
    SRCS SocketServiceImpl.cpp
    LIBS SocketServiceProto uuid_v4
    PUBLIC_INC $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES> # I should really fix this...
    STATIC
)

tgbot_exe(
    NAME SocketClient
    SRCS SocketServiceClient.cpp
    LIBS SocketServiceProto
    RELATION Socket
)