############### Bot functions' Test Arena (googletest) ##############
###################### Creates the test target ######################

add_my_executable(
  NAME test
  SRCS
    TestMain.cpp
    AuthorizationTest.cpp
    RegexHandlerTest.cpp
    ResourceManagerTest.cpp
    TryParseTest.cpp
    SharedMallocTest.cpp
    commands/CommandModulesTest.cpp
    commands/AliveCmdTest.cpp
    #commands/BashCmdTest.cpp
    commands/CmdCmdTest.cpp
    commands/DatabaseCmdTest.cpp
    commands/DeleteEchoCmdTest.cpp
  LIBS
    GTest::gtest
    GTest::gmock
    TgBotPPImpl fruit
)

# Add the test executable to CTest
add_test(NAME TestSuite COMMAND TgBot++_test)

add_my_executable(
  NAME test_database
  SRCS
    TestMain.cpp
    DatabaseBackendsTest.cpp
  LIBS
    GTest::gtest
    TgBotDBImpl
)
# Add the test executable to CTest
add_test(NAME TestSuiteDatabase COMMAND TgBot++_test_database)

add_my_executable(
  NAME test_socketdatahandler
  SRCS
    TestMain.cpp
    SocketDataHandlerTest.cpp
  LIBS
    GTest::gtest
    GTest::gmock
    TgBotPPImpl
    fruit
)
# Add the test executable to CTest
add_test(NAME TestSuiteSocketDataHandler COMMAND TgBot++_test_socketdatahandler)