# TgBotSocket library

if (UNIX)
  set(TGBOTSOCKET_SRC_LIST
    impl/SocketPosix.cpp
    impl/local/SocketPosixLocal.cpp
    impl/inet/SocketPosixIPv4.cpp
    impl/inet/SocketPosixIPv6.cpp
    impl/helper/SocketHelperPosix.cpp
    impl/backends/ClientBackendPosix.cpp
  )
  if (APPLE)
    list(APPEND TGBOTSOCKET_SRC_LIST
      impl/helper/SocketHelperDarwin.cpp
    )
  elseif(LINUX OR ANDROID)
    list(APPEND TGBOTSOCKET_SRC_LIST
      impl/helper/SocketHelperLinux.cpp
    )
  endif()
elseif(WIN32)
  list(APPEND TGBOTSOCKET_SRC_LIST 
    impl/SocketWindows.cpp
    impl/local/SocketWindowsLocal.cpp
    impl/inet/SocketWindowsIPv4.cpp
    impl/inet/SocketWindowsIPv6.cpp
    impl/helper/SocketHelperWindows.cpp
    impl/backends/ClientBackendWindows.cpp
  )
endif()
list(APPEND TGBOTSOCKET_SRC_LIST
  SocketBase.cpp
  impl/helper/SocketHelper.cpp
  impl/bot/TgBotPacketParser.cpp
  impl/bot/TgBotSocketFileHelperNew.cpp)

find_package(CURL)
set(CURL_LIBS)
if (CURL_FOUND)
  list(APPEND TGBOTSOCKET_SRC_LIST
    impl/helper/SocketHelperCurl.cpp)
  set(CURL_LIBS CURL::libcurl)
else()
  list(APPEND TGBOTSOCKET_SRC_LIST
    impl/helper/SocketHelperNoCurl.cpp)
endif()

add_my_library(
  NAME Socket
  SRCS ${TGBOTSOCKET_SRC_LIST}
  PUBLIC_INC ../include
  LIBS TgBotUtils TgBotSocketSelectors TgBotSocketCommandMap ${CURL_LIBS} ZLIB::ZLIB OpenSSL::Crypto
  LIBS_WIN32 wsock32 Ws2_32
)
