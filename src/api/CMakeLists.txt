find_package(LibXml2 REQUIRED)

option(ENABLE_LUA_COMMANDS "Enable Lua-based modules" ON)

set(liblua_srcs
liblua/lapi.c
liblua/lauxlib.c
liblua/lbaselib.c
liblua/lcode.c
liblua/lcorolib.c
liblua/lctype.c
liblua/ldblib.c
liblua/ldebug.c
liblua/ldo.c
liblua/ldump.c
liblua/lfunc.c
liblua/lgc.c
liblua/linit.c
liblua/liolib.c
liblua/llex.c
liblua/lmathlib.c
liblua/lmem.c
liblua/loadlib.c
liblua/lobject.c
liblua/lopcodes.c
liblua/loslib.c
liblua/lparser.c
liblua/lstate.c
liblua/lstring.c
liblua/lstrlib.c
liblua/ltable.c
liblua/ltablib.c
liblua/ltests.c
liblua/ltm.c
liblua/lua.c
liblua/lundump.c
liblua/lutf8lib.c
liblua/lvm.c
liblua/lzio.c
liblua/onelua.c)

set(lua_src)
set(lua_libs)
set(lua_def)
if (ENABLE_LUA_COMMANDS)
  message(STATUS "Enabling LuaCommandModule")
  add_library(tglua ${liblua_srcs})
  set(lua_src LuaCommandModule.cpp)
  set(lua_libs sol2::sol2 tglua)
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
