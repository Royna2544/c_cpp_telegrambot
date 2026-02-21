tgbot_library(
  NAME restartfmt_parser
  SRCS restartfmt_parser.cpp
  ALWAYS_STATIC
)

target_include_directories(restartfmt_parser PRIVATE $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>)