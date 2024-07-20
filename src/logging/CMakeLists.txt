add_my_library(
  NAME LogInit
  SRCS AbslLogInit.cpp
  LIBS absl::log_initialize absl::log_sink_registry
  PUBLIC_INC .
)

if (USE_UNIX_SOCKETS)
  add_my_executable(
    NAME logcat
    SRCS LoggingClient.cpp
    LIBS TgBotSocket
  )
endif()