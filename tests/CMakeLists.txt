############### Bot functions' Test Arena (googletest) ##############
###################### Creates the test target ######################
add_my_executable(
  NAME test
  SRCS
    TestMain.cpp
    AuthorizationTest.cpp
    MessageWrapperTest.cpp
    ConfigManagerTest.cpp
    RegexHandlerTest.cpp
    ResourceManagerTest.cpp
    PythonClassTest.cpp
    TryParseTest.cpp
    SharedMallocTest.cpp
    ConstexprStringCatTest.cpp
    CommandModulesTest.cpp
    SocketDataHandlerTest.cpp
  LIBS
    gtest
    gmock
    TgBotPPImpl
    TgBotPython
)
