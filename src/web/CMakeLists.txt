tgbot_library(
  NAME Web 
  SRCS WebServerBase.cpp
  LIBS httplib::httplib nlohmann_json::nlohmann_json
  STATIC
)