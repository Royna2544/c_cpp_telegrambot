set(LOGCAT_NAME ${PROJECT_NAME}_logcat)

add_my_library(
  NAME LogInit
  SRCS AbslLogInit.cpp
  LIBS absl::log_initialize absl::log_sink_registry
  PUBLIC_INC .
)

if (USE_UNIX_SOCKETS)
  add_executable_san(${LOGCAT_NAME} LoggingClient.cpp)
  target_link_libraries(${LOGCAT_NAME} TgBotSocket TgBotLogInit)
endif()