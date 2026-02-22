tgbot_exe(
  NAME main
  SRCS
    TestMain.cpp
    AuthorizationTest.cpp
    commands/CommandModulesTest.cpp
    commands/AliveCmdTest.cpp
    commands/CmdCmdTest.cpp
    commands/DatabaseCmdTest.cpp
    commands/DeleteEchoCmdTest.cpp
    commands/FileIdCmdTest.cpp
  TEST
)
target_link_libraries(test_main PRIVATE Api fruit GTest::gtest GTest::gmock Utils)
target_include_directories(test_main PRIVATE $<TARGET_PROPERTY:TgBot,INTERFACE_INCLUDE_DIRECTORIES>)

tgbot_exe(
  NAME database
  SRCS
    TestMain.cpp
    DatabaseBackendsTest.cpp
  TEST
)
target_link_libraries(test_database PRIVATE GTest::gtest DBImpl Utils)

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

tgbot_exe(
  NAME rombuildservice
  SRCS
    TestMain.cpp
    ROMBuildServiceTest.cpp
  TEST
)
target_link_libraries(test_rombuildservice PRIVATE ROMBuildServiceProto GTest::gtest GTest::gmock)
