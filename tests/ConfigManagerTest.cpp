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
