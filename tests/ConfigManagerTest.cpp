#include <ConfigManager.h>
#include <gtest/gtest.h>

#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 0
#endif

TEST(ConfigManagerTest, GetVariableEnv) {
    using namespace ConfigManager;
    ConfigManager::setVariable(Configs::OVERRIDE_CONF, "HELP");
    ConfigManager::setVariable(Configs::HELP, "VAR_VALUE");
    auto it = ConfigManager::getVariable(Configs::HELP);
    ASSERT_TRUE(it.has_value());
    EXPECT_EQ(it.value(), "VAR_VALUE");
}

TEST(ConfigManagerTest, CopyCommandLine) {
    int in_argc = 2;
    std::string buf = "ConfigManagerTest";
    std::string buf2 = "test";
    char *const ins_argv[] = {buf.data(), buf2.data()};
    char *const *ins_argv2 = ins_argv;
    copyCommandLine(CommandLineOp::INSERT, &in_argc, &ins_argv2);

    char *const *out_argv = nullptr;
    int out_argc = 0;
    copyCommandLine(CommandLineOp::GET, &out_argc, &out_argv);
    EXPECT_EQ(out_argc, 2);
    EXPECT_STREQ(out_argv[0], ins_argv[0]);
    EXPECT_STREQ(out_argv[1], ins_argv[1]);
}
