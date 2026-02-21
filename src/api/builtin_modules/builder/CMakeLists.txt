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
  NAME CurlDownload
  SRCS CurlUtils.cpp
  ALWAYS_STATIC
)
target_link_libraries(CurlDownload PRIVATE CURL::libcurl sighandler)

