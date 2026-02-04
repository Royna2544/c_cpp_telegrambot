add_subdirectory(kernel)

include(gRPCGenerate)
grpc_generate(SystemMonitor)
grpc_generate(HealthCheck)
add_subdirectory(android)

find_package(CURL)

if(NOT CURL_FOUND)
  message(STATUS "Builder: No cURL")
  return()
endif()

tgbot_library(
  NAME
  CurlDownload
  SRCS
  CurlUtils.cpp
  LIBS
  CURL::libcurl
  sighandler
  STATIC)

