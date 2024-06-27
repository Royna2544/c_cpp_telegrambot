# TgBotSocket library
set(SOCKET_CLI_NAME ${PROJECT_NAME}_SocketCli)
set(SOCKET_SRC_INTERFACE src/socket/interface)

if (UNIX)
  set(SOCKET_SRC_LIST
    ${SOCKET_SRC_INTERFACE}/impl/SocketPosix.cpp
    ${SOCKET_SRC_INTERFACE}/impl/local/SocketPosixLocal.cpp
    ${SOCKET_SRC_INTERFACE}/impl/inet/SocketPosixIPv4.cpp
    ${SOCKET_SRC_INTERFACE}/impl/inet/SocketPosixIPv6.cpp
    src/socket/selector/SelectorPosixPoll.cpp
    src/socket/selector/SelectorPosixSelect.cpp
    src/socket/selector/SelectorUnix.cpp
    ${SOCKET_SRC_INTERFACE}/impl/bot/ClientBackendPosix.cpp
  )
  if (APPLE)
    extend_set(SOCKET_SRC_LIST
      ${SOCKET_SRC_INTERFACE}/impl/helper/SocketHelperDarwin.cpp
      src/socket/selector/SelectorLinuxEPollStub.cpp
    )
  else()
    extend_set(SOCKET_SRC_LIST
      ${SOCKET_SRC_INTERFACE}/impl/helper/SocketHelperLinux.cpp
      src/socket/selector/SelectorLinuxEPoll.cpp
    )
  endif()
elseif(WIN32)
  extend_set(SOCKET_SRC_LIST 
    ${SOCKET_SRC_INTERFACE}/impl/SocketWindows.cpp
    ${SOCKET_SRC_INTERFACE}/impl/local/SocketWindowsLocal.cpp
    ${SOCKET_SRC_INTERFACE}/impl/inet/SocketWindowsIPv4.cpp
    ${SOCKET_SRC_INTERFACE}/impl/inet/SocketWindowsIPv6.cpp
    ${SOCKET_SRC_INTERFACE}/impl/helper/SocketHelperWindows.cpp
    src/socket/selector/SelectorWindowsSelect.cpp
    ${SOCKET_SRC_INTERFACE}/impl/bot/ClientBackendWindows.cpp
  )
endif()
extend_set(SOCKET_SRC_LIST
  ${SOCKET_SRC_INTERFACE}/impl/helper/SocketHelper.cpp)

if (CURL_FOUND)
  extend_set(SOCKET_SRC_LIST
    ${SOCKET_SRC_INTERFACE}/impl/helper/SocketHelperCurl.cpp)
else()
  extend_set(SOCKET_SRC_LIST
    ${SOCKET_SRC_INTERFACE}/impl/helper/SocketHelperNoCurl.cpp)
endif()
add_library_san(TgBotSocket SHARED
  ${SOCKET_SRC_LIST}
  ${SOCKET_SRC_INTERFACE}/SocketBase.cpp
  ${SOCKET_SRC_INTERFACE}/impl/bot/TgBotPacketParser.cpp
  src/socket/TgBotCommandMap.cpp)
target_include_directories(TgBotSocket PRIVATE ${CURL_INCLUDE_DIRS})
target_include_directories(TgBotSocket PUBLIC src/socket/include ${SOCKET_SRC_INTERFACE})
if (CURL_FOUND)
  target_link_libraries(TgBotSocket CURL::libcurl)
endif()
target_link_libraries(TgBotSocket TgBotUtils)
add_executable_san(${SOCKET_CLI_NAME}
  src/socket/TgBotSocketClient.cpp)

target_link_libraries(${SOCKET_CLI_NAME} TgBotSocket TgBotLogInit)
target_link_lib_if_windows(${SOCKET_CLI_NAME} Ws2_32)