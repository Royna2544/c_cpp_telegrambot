tgbot_library(
  NAME
  restartfmt_parser
  SRCS
  restartfmt_parser.cpp
  STATIC
  PRIVATE_INC
  $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>)
