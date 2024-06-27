############### Bot functions' Test Arena (googletest) ##############
###################### Creates the test target ######################
add_executable_san(${PROJECT_TEST_NAME}
  tests/TestMain.cpp
  tests/AuthorizationTest.cpp
  tests/MessageWrapperTest.cpp
  tests/ConfigManagerTest.cpp
  tests/RegexHandlerTest.cpp
  tests/DatabaseLoader.cpp
  tests/ResourceManagerTest.cpp
  tests/TryParseTest.cpp
  tests/SharedMallocTest.cpp
  tests/ConstexprStringCatTest.cpp
)
target_include_directories(${PROJECT_TEST_NAME} PRIVATE ${gtest_INCLUDE_DIR} ${gmock_INCLUDE_DIR})
target_link_libraries(${PROJECT_TEST_NAME} gtest gmock ${PROJECT_NAME} TgBotLogInit)
add_test(NAME ${PROJECT_TEST_NAME} COMMAND ${PROJECT_TEST_NAME})
#####################################################################