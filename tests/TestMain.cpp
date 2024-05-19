#include <gtest/gtest.h>
#include <AbslLogInit.hpp>

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    TgBot_AbslLogInit();
    return RUN_ALL_TESTS();
}