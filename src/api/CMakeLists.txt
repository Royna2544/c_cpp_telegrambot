find_package(LibXml2 REQUIRED)

tgbot_library(
  NAME Api
  SRCS
    Authorization.cpp
    TgBotApiImpl.cpp
    CommandModule.cpp
    MessageExt.cpp
    RateLimit.cpp
    StringResLoader.cpp
    LIBS DBImpl ${CMAKE_DL_LIBS} TgBot LibXml2::LibXml2 cpptrace::cpptrace
  STATIC
)

add_subdirectory(components)
