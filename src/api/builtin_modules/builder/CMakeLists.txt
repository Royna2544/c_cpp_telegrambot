add_subdirectory(kernel)

include(gRPCGenerate)
grpc_generate(SystemMonitor)
grpc_generate(HealthCheck)

# Shared, Telegram-free builder infrastructure (observer, stream wrapper,
# build state, system stats). Header-only pieces are exposed via the PUBLIC
# include directory so any module/test that links BuilderCommon can use them.
tgbot_library(
  NAME BuilderCommon
  SRCS SystemStatsProvider.cpp
  ALWAYS_STATIC
)
target_link_libraries(BuilderCommon PUBLIC SystemMonitorServiceProto)
target_include_directories(BuilderCommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

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

