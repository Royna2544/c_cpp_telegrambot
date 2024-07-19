set(PROJECT_TEST_NAME ${PROJECT_NAME}_test)
############### Bot functions' Test Arena (googletest) ##############
###################### Creates the test target ######################
add_executable_san(${PROJECT_TEST_NAME}
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
)
target_include_directories(${PROJECT_TEST_NAME} PRIVATE ${gtest_INCLUDE_DIR} ${gmock_INCLUDE_DIR})
target_link_libraries(${PROJECT_TEST_NAME} gtest gmock TgBotPPImpl TgBotLogInit TgBotPython)
