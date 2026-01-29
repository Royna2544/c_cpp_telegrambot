find_package(LibXml2 REQUIRED)

option(TGBOTCPP_LUA_MODULES "Enable Lua-based modules" ON)

# Enable LuaCommandModule if sol2 is found
find_package(sol2)
set(lua_src)
set(lua_libs)
set(lua_def)
if (TGBOTCPP_LUA_MODULES AND sol2_FOUND)
  message(STATUS "Enabling LuaCommandModule")
  add_library(tglua STATIC liblua/onelua.c)
  target_compile_definitions(tglua PRIVATE MAKE_LIB)
  target_include_directories(tglua INTERFACE liblua)
  set(lua_src LuaCommandModule.cpp)
  set(lua_libs sol2::sol2 tglua)
  set(lua_def HAVE_LUA)
endif()

# Optional: cpptrace for backtraces
find_package(cpptrace)
set(cpptrace_def)
set(cpptrace_libs)
if (cpptrace_FOUND)
  message(STATUS "Enabling cpptrace for backtraces in TgBotApiImpl")
  set(cpptrace_def TGBOTCPP_ENABLE_CPPTRACE)
  set(cpptrace_libs cpptrace::cpptrace)
endif()

tgbot_library(
  NAME Api
  SRCS
    AuthContext.cpp
    DynCommandModule.cpp
    ${lua_src}
    MessageExt.cpp
    RateLimit.cpp
    StringResLoader.cpp
  LIBS
    TgBot # TODO: This will be removed after refactoring
    DBImpl # Authorization
    ${CMAKE_DL_LIBS} # CommandModule
    restartfmt_parser # RestartCommand
    LibXml2::LibXml2 # StringResLoader
    ${lua_libs}
  DEFS ${lua_def}
  STATIC
)

tgbot_library(
  NAME ApiImpl
  SRCS 
    TgBotApiImpl.cpp
    components/Async.cpp
    components/ChatJoinRequest.cpp
    components/ModuleManagement.cpp
    components/OnAnyMessage.cpp
    components/OnCallbackQuery.cpp
    components/OnInlineQuery.cpp
    components/OnMyChatMember.cpp
    components/Restart.cpp
    components/UnknownCommand.cpp
    components/ReactionsProvider.cpp
  LIBS TgBot DBImpl ${cpptrace_libs} Api
  DEFS ${cpptrace_def}
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
add_subdirectory(net)
