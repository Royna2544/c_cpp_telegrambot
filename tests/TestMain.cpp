#include <gtest/gtest.h>
#include <AbslLogInit.hpp>
#include "database/bot/TgBotDatabaseImpl.hpp"
#include <CommandLine.hpp>

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    TgBot_AbslLogInit();
    CommandLine::initInstance(argc, argv);
    const auto dbImpl = TgBotDatabaseImpl::getInstance();
    dbImpl->loadDBFromConfig();
    int ret = RUN_ALL_TESTS();
    dbImpl->unloadDatabase();
    return ret;
}
