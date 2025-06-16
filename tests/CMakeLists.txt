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
  LIBS
    GTest::gtest
    GTest::gmock
    Api
    fruit
  TEST
)

tgbot_exe(
  NAME database
  SRCS
    TestMain.cpp
    DatabaseBackendsTest.cpp
  LIBS
    GTest::gtest
    DBImpl
  TEST
)

tgbot_exe(
  NAME regex
  SRCS
    TestMain.cpp
    RegexHandlerTest.cpp
  LIBS
    GTest::gtest
    GTest::gmock
    Regex
  TEST
)

tgbot_exe(
  NAME socketdatahandler
  SRCS
    TestMain.cpp
    SocketDataHandlerTest.cpp
  LIBS
    GTest::gtest
    GTest::gmock
    SocketComponents
    fruit
    HexDump
  TEST
)

tgbot_exe(
  NAME popenwdt
  SRCS
    TestMain.cpp
    PopenWdtTest.cpp
  LIBS
    GTest::gtest
    popen_wdt
  TEST
)
