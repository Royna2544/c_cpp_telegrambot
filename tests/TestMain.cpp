#include <gtest/gtest.h>

#include <AbslLogInit.hpp>
#include <CommandLine.hpp>
#include <StringResLoader.hpp>

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    TgBot_AbslLogInit();
    CommandLine::initInstance(argc, argv);
    int ret = RUN_ALL_TESTS();
    return ret;
}