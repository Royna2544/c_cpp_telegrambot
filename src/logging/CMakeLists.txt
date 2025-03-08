if (WIN32) 
tgbot_library(
  NAME LogInit
  SRCS AbslLogInit.cpp
  LIBS absl::log_initialize absl::log_sink_registry
  STATIC
)
else()
tgbot_library(
  NAME LogInit
  SRCS AbslLogInit.cpp
  LIBS absl::log_initialize absl::log_sink_registry
)
endif()

tgbot_exe(
  NAME logcat
  SRCS LoggingClient.cpp
  LIBS Socket Utils sighandler
  RELATION Socket
)