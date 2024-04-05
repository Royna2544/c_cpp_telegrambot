#include <gtest/gtest.h>
#include <absl/log/initialize.h>

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    absl::InitializeLog();
    return RUN_ALL_TESTS();
}