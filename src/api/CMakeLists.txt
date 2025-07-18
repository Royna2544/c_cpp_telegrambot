find_package(LibXml2 REQUIRED)

option(ENABLE_LUA_COMMANDS "Enable Lua-based modules" ON)

set(lua_src)
set(lua_libs)
set(lua_def)
if (ENABLE_LUA_COMMANDS)
  message(STATUS "Enabling LuaCommandModule")
  add_library(tglua liblua/onelua.c)
  target_compile_definitions(tglua PRIVATE MAKE_LIB)
  target_include_directories(tglua INTERFACE liblua)
  set(lua_src LuaCommandModule.cpp)
  set(lua_libs sol2::sol2 tglua)
  set(lua_def HAVE_LUA)
endif()

tgbot_library(
  NAME Api
  SRCS
    AuthContext.cpp
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
  SRCS AuthContext.cpp
  LIBS DBImpl
  PUBLIC_INC $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>
  STATIC
)

add_subdirectory(components/restartfmt_parser)
