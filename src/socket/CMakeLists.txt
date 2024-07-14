add_subdirectory(selector)
add_subdirectory(interface)

add_my_library(
  NAME SocketCommandMap 
  SRCS TgBotCommandMap.cpp
  PRIVATE_INC include)

set(SOCKET_CLI_NAME ${PROJECT_NAME}_SocketCli)
add_executable_san(${SOCKET_CLI_NAME}
  TgBotSocketClient.cpp)
target_link_libraries(${SOCKET_CLI_NAME} TgBotSocket TgBotLogInit)
target_link_lib_if_windows(${SOCKET_CLI_NAME} Ws2_32)