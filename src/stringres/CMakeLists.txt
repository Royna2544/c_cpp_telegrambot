find_package(LibXml2 REQUIRED)

# TgBotStringRes library
add_my_library(
  NAME StringRes 
  SRCS StringResLoader.cpp
  LIBS LibXml2::LibXml2
  STATIC
)

add_my_executable(
  NAME StringResLoadTest
  SRCS StringResCompiler.cpp
  LIBS TgBotStringRes
  NO_PREFIX
  OPTIONAL
)
