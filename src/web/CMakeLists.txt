tgbot_library(
  NAME Web 
  SRCS WebServerBase.cpp
  ALWAYS_STATIC
)

target_link_libraries(Web PRIVATE httplib::httplib nlohmann_json::nlohmann_json SocketServiceProto SystemMonitorServiceProto)