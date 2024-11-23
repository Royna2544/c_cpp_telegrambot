# TgBotSocket library

find_package(Boost CONFIG COMPONENTS system)
add_my_library(
  NAME Socket
  SRCS
    Context.cpp
    Local.cpp
    UDP.cpp
    TCP.cpp
    bot/TgBotPacketParser.cpp
    bot/TgBotSocketFileHelperNew.cpp
    backends/ClientBackend.cpp
  PUBLIC_INC ../include
  LIBS TgBotUtils TgBotSocketSelectors TgBotSocketCommandMap ${CURL_LIBS} ZLIB::ZLIB OpenSSL::Crypto Boost::system
)
