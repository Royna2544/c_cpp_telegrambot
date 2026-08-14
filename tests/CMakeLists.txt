tgbot_exe(
  NAME main
  SRCS
    TestMain.cpp
    AuthorizationTest.cpp
    AskConfirmToolTest.cpp
    BuilderLaunchToolTest.cpp
    ToolRouterTest.cpp
    commands/CommandModulesTest.cpp
    commands/AliveCmdTest.cpp
    commands/AskCmdTest.cpp
    commands/CmdCmdTest.cpp
    commands/DatabaseCmdTest.cpp
    commands/DecidePossibilityCmdTest.cpp
    commands/DeleteEchoCmdTest.cpp
    commands/FileIdCmdTest.cpp
    commands/RotatePicCmdTest.cpp
    commands/ScheduledCommandsTest.cpp
  TEST
)
target_link_libraries(test_main PRIVATE Api fruit GTest::gtest GTest::gmock Utils)
target_include_directories(test_main PRIVATE
  ${CMAKE_SOURCE_DIR}/src/command_modules
  ${CMAKE_CURRENT_SOURCE_DIR}
  $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>)
add_dependencies(test_main cmd_ask cmd_bash cmd_c cmd_cpp cmd_decide cmd_flash
                 cmd_possibility cmd_q cmd_rotatepic cmd_spam cmd_ubash)
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_dependencies(test_main cmd_py)
  target_compile_definitions(test_main PRIVATE TEST_HAVE_CMD_PY)
endif()
if(UNIX)
  add_dependencies(test_main cmd_ibash)
  target_compile_definitions(test_main PRIVATE TEST_HAVE_CMD_IBASH)
endif()

configure_file(tsan-pango.supp tsan-pango.supp COPYONLY)

tgbot_exe(
  NAME random
  SRCS
    TestMain.cpp
    RandomTest.cpp
  TEST
)
target_link_libraries(test_random PRIVATE GTest::gtest Random)

tgbot_exe(
  NAME commanddispatch
  SRCS
    TestMain.cpp
    CommandDispatchTest.cpp
    ${CMAKE_SOURCE_DIR}/src/ThreadManager.cpp
  TEST
)
target_link_libraries(test_commanddispatch PRIVATE GTest::gtest ApiImpl)
target_include_directories(test_commanddispatch PRIVATE
  ${CMAKE_SOURCE_DIR}/src/include
  $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>)

tgbot_exe(
  NAME workscheduler
  SRCS
    TestMain.cpp
    WorkSchedulerTest.cpp
    ${CMAKE_SOURCE_DIR}/src/api/components/Async.cpp
    ${CMAKE_SOURCE_DIR}/src/api/components/WorkScheduler.cpp
  TEST
)
target_link_libraries(test_workscheduler PRIVATE GTest::gtest Api)
target_include_directories(test_workscheduler PRIVATE
  ${CMAKE_SOURCE_DIR}/src/include
  $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>)

tgbot_exe(
  NAME database
  SRCS
    TestMain.cpp
    DatabaseBackendsTest.cpp
  TEST
)
target_link_libraries(test_database PRIVATE GTest::gtest DBImpl Utils)
find_package(SQLite3 QUIET)
if(SQLite3_FOUND)
  target_link_libraries(test_database PRIVATE SQLite::SQLite3)
endif()

tgbot_exe(
  NAME regex
  SRCS
    TestMain.cpp
    RegexHandlerTest.cpp
  TEST
)
target_link_libraries(test_regex PRIVATE GTest::gtest GTest::gmock Regex)

tgbot_exe(
  NAME popenwdt
  SRCS
    TestMain.cpp
    PopenWdtTest.cpp
  TEST
)
target_link_libraries(test_popenwdt PRIVATE popen_wdt GTest::gtest)

tgbot_exe(
  NAME compilerbase
  SRCS
    TestMain.cpp
    CompilerBaseTest.cpp
  TEST
)
target_link_libraries(test_compilerbase PRIVATE Compiler GTest::gtest)
target_include_directories(test_compilerbase PRIVATE
  ${CMAKE_SOURCE_DIR}/src/command_modules
  $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>)

tgbot_exe(
  NAME stringresloader
  SRCS
    TestMain.cpp
    StringResLoaderTest.cpp
  TEST
)
target_link_libraries(test_stringresloader PRIVATE GTest::gtest Api Utils)

tgbot_exe(
  NAME kernelbuildservice
  SRCS
    TestMain.cpp
    LinuxKernelBuildServiceTest.cpp
  TEST
)
target_link_libraries(test_kernelbuildservice PRIVATE LinuxKernelBuildServiceProto GTest::gtest GTest::gmock)
target_include_directories(test_kernelbuildservice PRIVATE ${CMAKE_SOURCE_DIR}/src/api/builtin_modules/builder)

tgbot_exe(
  NAME rombuildservice
  SRCS
    TestMain.cpp
    ROMBuildServiceTest.cpp
  TEST
)
target_link_libraries(test_rombuildservice PRIVATE ROMBuildServiceProto GTest::gtest GTest::gmock)
target_include_directories(test_rombuildservice PRIVATE ${CMAKE_SOURCE_DIR}/src/api/builtin_modules/builder)

tgbot_exe(
  NAME llmadapters
  SRCS
    TestMain.cpp
    LLMAdaptersTest.cpp
  TEST
)
target_link_libraries(test_llmadapters PRIVATE GTest::gtest CurlDownload)
target_include_directories(test_llmadapters PRIVATE
  ${CMAKE_SOURCE_DIR}/src/command_modules
  ${CMAKE_SOURCE_DIR}/src/api/builtin_modules/builder)

tgbot_exe(
  NAME ratelimit
  SRCS
    TestMain.cpp
    RateLimitTest.cpp
  TEST
)
target_link_libraries(test_ratelimit PRIVATE GTest::gtest RateLimitApi)
target_include_directories(test_ratelimit PRIVATE ${CMAKE_SOURCE_DIR}/src/include)

tgbot_exe(
  NAME markdownv2
  SRCS
    TestMain.cpp
    MarkdownV2Test.cpp
  TEST
)
target_link_libraries(test_markdownv2 PRIVATE GTest::gtest absl::strings)
target_include_directories(test_markdownv2 PRIVATE ${CMAKE_SOURCE_DIR}/src/include)

tgbot_exe(
  NAME restartfmt
  SRCS
    TestMain.cpp
    RestartFmtTest.cpp
  TEST
)
target_link_libraries(test_restartfmt PRIVATE GTest::gtest restartfmt_parser absl::strings Utils)
target_include_directories(test_restartfmt PRIVATE
  ${CMAKE_SOURCE_DIR}/src/api/components/restartfmt_parser
  ${CMAKE_SOURCE_DIR}/src/include
  ${CMAKE_SOURCE_DIR}/src
  $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>)

tgbot_exe(
  NAME spamblock
  SRCS
    TestMain.cpp
    SpamBlockTest.cpp
    ${CMAKE_SOURCE_DIR}/src/global_handlers/SpamBlocker.cpp
  TEST
)
target_link_libraries(test_spamblock PRIVATE GTest::gtest TgBot absl::log fmt::fmt-header-only)
target_include_directories(test_spamblock PRIVATE
  ${CMAKE_SOURCE_DIR}/src/include
  ${CMAKE_SOURCE_DIR}/src
  $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>)

tgbot_exe(
  NAME filetransfer
  SRCS
    TestMain.cpp
    FileTransferValidationTest.cpp
  TEST
)
target_link_libraries(test_filetransfer PRIVATE GTest::gtest)
target_include_directories(test_filetransfer PRIVATE ${CMAKE_SOURCE_DIR}/src/api/net)

tgbot_exe(
  NAME corecorrectness
  SRCS
    TestMain.cpp
    CoreCorrectnessTest.cpp
  TEST
)
target_link_libraries(test_corecorrectness PRIVATE GTest::gtest)
target_include_directories(test_corecorrectness PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/src/include)

tgbot_exe(
  NAME quoterenderer
  SRCS
    TestMain.cpp
    QuoteRendererTest.cpp
  TEST
)
find_package(PNG REQUIRED)
target_link_libraries(test_quoterenderer PRIVATE GTest::gtest PNG::PNG
                                                    QuoteRenderer)
target_include_directories(test_quoterenderer PRIVATE ${CMAKE_SOURCE_DIR}/src/include)

tgbot_exe(
  NAME qcommand
  SRCS
    TestMain.cpp
    commands/CommandModulesTest.cpp
    commands/QCmdTest.cpp
  TEST
)
target_link_libraries(test_qcommand PRIVATE Api fruit GTest::gtest GTest::gmock
                                            Utils)
target_include_directories(test_qcommand PRIVATE
  ${CMAKE_SOURCE_DIR}/src/command_modules
  ${CMAKE_CURRENT_SOURCE_DIR}
  $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>)
add_dependencies(test_qcommand cmd_q)
