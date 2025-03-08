tgbot_library(
  NAME ApiComponents
  SRCS
    OnAnyMessage.cpp
    ChatJoinRequest.cpp
    OnCallbackQuery.cpp
    OnInlineQuery.cpp
    OnMyChatMember.cpp
    FileCheck.cpp
    Async.cpp
    Restart.cpp
    ModuleManagement.cpp
    UnknownCommand.cpp
  LIBS restartfmt_parser CURL::libcurl Api TgBot
  STATIC
)