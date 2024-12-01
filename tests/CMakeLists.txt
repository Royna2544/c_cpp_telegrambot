############### Bot functions' Test Arena (googletest) ##############
###################### Creates the test target ######################

add_my_executable(
  NAME main
  SRCS
    TestMain.cpp
    AuthorizationTest.cpp
    commands/CommandModulesTest.cpp
    commands/AliveCmdTest.cpp
    #commands/BashCmdTest.cpp
    commands/CmdCmdTest.cpp
    commands/DatabaseCmdTest.cpp
    commands/DeleteEchoCmdTest.cpp
  LIBS
    GTest::gtest
    GTest::gmock
    TestingFileGroup fruit TgBotRandom
  TEST
)

add_my_executable(
  NAME database
  SRCS
    TestMain.cpp
    DatabaseBackendsTest.cpp
  LIBS
    GTest::gtest
    DBImpl
  TEST
)

add_my_executable(
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

add_my_executable(
  NAME socketdatahandler
  SRCS
    TestMain.cpp
    SocketDataHandlerTest.cpp
  LIBS
    GTest::gtest
    GTest::gmock
    TestingFileGroup
    fruit
  TEST
)

add_my_executable(
  NAME popenwdt
  SRCS
    TestMain.cpp
    PopenWdtTest.cpp
  LIBS
    GTest::gtest
    popen_wdt
  TEST
)
