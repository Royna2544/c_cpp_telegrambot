#include <gtest/gtest.h>

#include "ResourceManager.h"

class ResourceManagerTest : public testing::Test {
   protected:
    void SetUp() override { gResourceManager.preloadResourceDirectory(); }
    ResourceManager& gResourceManager = ResourceManager::getInstance();
};

TEST_F(ResourceManagerTest, PreloadOneFile) {
    const std::string expected = "This is a test file";
    const std::string_view actual =
        gResourceManager.getResource("test/test.txt");
    EXPECT_EQ(expected, actual);
}

TEST_F(ResourceManagerTest, PreloadAgain) {
    bool rc = gResourceManager.preloadOneFile(
        ResourceManager::getResourceRootdir() / "test/test.txt");
    EXPECT_FALSE(rc);
    rc = gResourceManager.preloadOneFile("test/test.txt");
    EXPECT_FALSE(rc);
}