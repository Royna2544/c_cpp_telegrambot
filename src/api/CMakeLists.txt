find_package(LibXml2 REQUIRED)

tgbot_library(
  NAME Api
  SRCS
    Authorization.cpp
    TgBotApiImpl.cpp
    CommandModule.cpp
    MessageExt.cpp
    RateLimit.cpp
    StringResLoader.cpp
    components/Async.cpp
    components/ChatJoinRequest.cpp
    components/ModuleManagement.cpp
    components/OnAnyMessage.cpp
    components/OnCallbackQuery.cpp
    components/OnInlineQuery.cpp
    components/OnMyChatMember.cpp
    components/Restart.cpp
    components/UnknownCommand.cpp
  LIBS
    DBImpl # Authorization
    ${CMAKE_DL_LIBS} # CommandModule
    restartfmt_parser # RestartCommand
    TgBot
    LibXml2::LibXml2 # StringResLoader
    cpptrace::cpptrace # TgBotApiImpl's backtrace
  STATIC
)

