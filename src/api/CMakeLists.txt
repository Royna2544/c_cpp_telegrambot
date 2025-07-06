find_package(LibXml2 REQUIRED)
find_library(HAVE_LIBLUA lua)

set(lua_src)
set(lua_libs)
set(lua_def)
if (HAVE_LIBLUA)
  message(STATUS "Lua found, enabling LuaCommandModule")
  set(lua_src LuaCommandModule.cpp)
  set(lua_libs sol2::sol2 lua)
  set(lua_def HAVE_LUA)
endif()

tgbot_library(
  NAME Api
  SRCS
    Authorization.cpp
    TgBotApiImpl.cpp
    DynCommandModule.cpp
    ${lua_src}
    MessageExt.cpp
    RateLimit.cpp
    StringResLoader.cpp
    components/Async.cpp
    components/ChatJoinRequest.cpp
    components/ModuleManagement.cpp
    components/OnAnyMessage.cpp
    components/OnCallbackQuery.cpp
    components/OnInlineQuery.cpp
    components/OnMyChatMember.cpp
    components/Restart.cpp
    components/UnknownCommand.cpp
  LIBS
    TgBot
    DBImpl # Authorization
    ${CMAKE_DL_LIBS} # CommandModule
    restartfmt_parser # RestartCommand
    LibXml2::LibXml2 # StringResLoader
    cpptrace::cpptrace # TgBotApiImpl's backtrace
    ${lua_libs}
  DEFS ${lua_def}
  STATIC
)

tgbot_library(
  NAME AuthApi
  SRCS Authorization.cpp
  LIBS DBImpl
  PUBLIC_INC $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>
  STATIC
)
