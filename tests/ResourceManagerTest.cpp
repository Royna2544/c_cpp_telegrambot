#include <gtest/gtest.h>

#include <libfs.hpp>
#include <memory>
#include <string_view>

#include "ResourceManager.h"

class ResourceManagerTest : public testing::Test {
   protected:
    std::unique_ptr<ResourceManager> const gResourceManager =
        std::make_unique<ResourceManager>();
    constexpr static std::string_view kResourceTestFile = "test.txt";
};

TEST_F(ResourceManagerTest, PreloadOneFile) {
    const std::string expected = "This is a test file";
    const std::string_view actual = gResourceManager->get(kResourceTestFile);
    EXPECT_EQ(expected, actual);
}

TEST_F(ResourceManagerTest, PreloadAgain) {
    bool rc = gResourceManager->preload(FS::getPath(FS::PathType::RESOURCES) /
                                        kResourceTestFile);
    EXPECT_FALSE(rc);
    rc = gResourceManager->preload(kResourceTestFile);
    EXPECT_FALSE(rc);
}