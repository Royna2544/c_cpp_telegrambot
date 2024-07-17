#include <gtest/gtest.h>
#include <AbslLogInit.hpp>
#include "database/bot/TgBotDatabaseImpl.hpp"
#include <CommandLine.hpp>
#include <StringResManager.hpp>

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    TgBot_AbslLogInit();
    CommandLine::initInstance(argc, argv);
    StringResManager::getInstance()->initWrapper();
    int ret = RUN_ALL_TESTS();
    return ret;
}
