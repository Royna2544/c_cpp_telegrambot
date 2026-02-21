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
  ALWAYS_STATIC
)
target_link_libraries(Api PRIVATE 
    TgBot # TODO: This will be removed after refactoring
    DBImpl # Authorization
    ${CMAKE_DL_LIBS} # CommandModule
    restartfmt_parser # RestartCommand
    LibXml2::LibXml2 # StringResLoader
    ${lua_libs}
)
target_compile_definitions(Api PRIVATE ${lua_def})

# Builtin modules
add_subdirectory(builtin_modules/builder)
tgbot_library(
  NAME builtin_module_RomBuild
  SRCS
    builtin_modules/builder/android/romBuild.cpp
  ALWAYS_STATIC
)
target_link_libraries(builtin_module_RomBuild PRIVATE AndroidBuilder-configparse Utils TgBot AuthApi ROMBuildServiceProto SystemMonitorServiceProto HealthCheckServiceProto)


tgbot_library(
  NAME builtin_module_KernelBuild
  SRCS
    builtin_modules/builder/kernel/kernelbuild.cpp
    builtin_modules/builder/kernel/ConfigParsers2.cpp
  ALWAYS_STATIC
)
target_link_libraries(builtin_module_KernelBuild PRIVATE
  AuthApi
  nlohmann_json::nlohmann_json
  Utils
  progress
  LinuxKernelBuildServiceProto
  SystemMonitorServiceProto
  HealthCheckServiceProto
  RateLimitApi
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
  ALWAYS_STATIC
)
target_link_libraries(ApiImpl PRIVATE TgBot DBImpl ${cpptrace_libs} Api builtin_module_KernelBuild builtin_module_RomBuild restartfmt_parser)
target_compile_definitions(ApiImpl PRIVATE ${cpptrace_def})

tgbot_library(
  NAME AuthApi
  SRCS AuthContext.cpp
  ALWAYS_STATIC
)
target_link_libraries(AuthApi PRIVATE DBImpl)
target_include_directories(AuthApi PUBLIC $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>)

tgbot_library(
  NAME RateLimitApi
  SRCS RateLimit.cpp
  ALWAYS_STATIC
)

add_subdirectory(components/restartfmt_parser)
add_subdirectory(net)
