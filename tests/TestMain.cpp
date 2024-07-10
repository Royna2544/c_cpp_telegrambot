#include <gtest/gtest.h>
#include <AbslLogInit.hpp>
#include "database/bot/TgBotDatabaseImpl.hpp"

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    TgBot_AbslLogInit();
    TgBotDatabaseImpl::getInstance()->loadDBFromConfig();
    int ret = RUN_ALL_TESTS();
    TgBotDatabaseImpl::getInstance()->unloadDatabase();
    return ret;
}
