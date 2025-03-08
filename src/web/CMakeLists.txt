tgbot_library(
  NAME Web 
  SRCS WebServerBase.cpp
  LIBS httplib::httplib JsonCpp::JsonCpp
  STATIC
)