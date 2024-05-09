#include <gtest/gtest.h>

#include <libos/libfs.hpp>
#include <memory>

#include "ResourceManager.h"

class ResourceManagerTest : public testing::Test {
   protected:
    std::shared_ptr<ResourceManager> gResourceManager =
        ResourceManager::getInstance();
    constexpr static const char kResourceTestFile[] = "test.txt";
    void SetUp() override { gResourceManager->preloadResourceDirectory(); }
};

TEST_F(ResourceManagerTest, PreloadOneFile) {
    const std::string expected = "This is a test file";
    const std::string_view actual =
        gResourceManager->getResource(kResourceTestFile);
    EXPECT_EQ(expected, actual);
}

TEST_F(ResourceManagerTest, PreloadAgain) {
    bool rc = gResourceManager->preloadOneFile(
        FS::getPathForType(FS::PathType::RESOURCES) / kResourceTestFile);
    EXPECT_FALSE(rc);
    rc = gResourceManager->preloadOneFile(kResourceTestFile);
    EXPECT_FALSE(rc);
}