find_package(LibXml2 REQUIRED)

add_my_library(
  NAME Api
  SRCS
    Authorization.cpp
    TgBotApiImpl.cpp
    CommandModule.cpp
    MessageExt.cpp
    RateLimit.cpp
    StringResLoader.cpp
    LIBS DBImpl ${CMAKE_DL_LIBS} TgBot LibXml2::LibXml2
  STATIC
)

add_subdirectory(components)
