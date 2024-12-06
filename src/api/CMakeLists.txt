add_my_library(
  NAME Api
  SRCS
    Authorization.cpp
    TgBotApiImpl.cpp
    CommandModule.cpp
    MessageExt.cpp
    RateLimit.cpp
  LIBS DBImpl ${CMAKE_DL_LIBS} StringRes TgBot
  STATIC
)

add_subdirectory(components)