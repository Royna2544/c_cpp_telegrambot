#include <gtest/gtest.h>
#include <ConfigManager.h>

class ConfigManagerTest : public ::testing::Test {
 protected:
  void SetUp() override {
    // Set up the mock environment variables
    setenv("VAR_NAME", "VAR_VALUE", 1);
  }

  void TearDown() override {
    // Remove the mock environment variables
    unsetenv("VAR_NAME");
  }
};

TEST_F(ConfigManagerTest, GetVariableEnv) {
  std::string value;
  EXPECT_TRUE(ConfigManager::getVariable("VAR_NAME", value));
  EXPECT_EQ(value, "VAR_VALUE");
}

TEST_F(ConfigManagerTest, CopyCommandLine) {
  int in_argc = 2;
  const char* ins_argv[] = {"ConfigManagerTest", "test"};
  const char** ins_argv2 = ins_argv;
  copyCommandLine(CommandLineOp::INSERT, &in_argc, &ins_argv2);

  const char ** out_argv = nullptr;
  int out_argc = 0;
  copyCommandLine(CommandLineOp::GET, &out_argc, &out_argv);
  EXPECT_EQ(out_argc, 2);
  EXPECT_STREQ(out_argv[0], ins_argv[0]);
  EXPECT_STREQ(out_argv[1], ins_argv[1]);
}
