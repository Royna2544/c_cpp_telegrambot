#include <gtest/gtest.h>
#include <AbslLogInit.hpp>
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

extern "C" const char* __asan_default_options() { return "detect_leaks=0"; }