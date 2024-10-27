############### Bot functions' Test Arena (googletest) ##############
###################### Creates the test target ######################
find_package(GTest)

if (NOT ${GTest_FOUND})
  message(STATUS "Disabling tests")
  return()
endif()

add_my_executable(
  NAME test
  SRCS
    TestMain.cpp
    AuthorizationTest.cpp
    DatabaseBackendsTest.cpp
    ConfigManagerTest.cpp
    RegexHandlerTest.cpp
    ResourceManagerTest.cpp
    TryParseTest.cpp
    SharedMallocTest.cpp
    ConstexprStringCatTest.cpp
    commands/CommandModulesTest.cpp
    commands/AliveCmdTest.cpp
    commands/BashCmdTest.cpp
    commands/CmdCmdTest.cpp
    commands/DatabaseCmdTest.cpp
    commands/DeleteEchoCmdTest.cpp
    SocketDataHandlerTest.cpp
  LIBS
    GTest::gtest
    GTest::gmock
    TgBotPPImpl
)
