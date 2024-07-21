############### Bot functions' Test Arena (googletest) ##############
###################### Creates the test target ######################
find_package(GTest)

if (NOT ${GTest_FOUND})
  message(STATUS "Disabling tests")
  return()
endif()

if (USE_UNIX_SOCKETS)
  set(SOCKET_TEST_FILE SocketDataHandlerTest.cpp)
else()
  set(SOCKET_TEST_FILE)
endif()

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
    ${SOCKET_TEST_FILE}
  LIBS
    GTest::gtest
    GTest::gmock
    TgBotPPImpl
    TgBotPython
)
