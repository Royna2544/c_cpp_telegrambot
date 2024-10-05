#include <gtest/gtest.h>

#include <AbslLogInit.hpp>
#include <CommandLine.hpp>
#include <StringResManager.hpp>

struct StringResLoaderFake : StringResLoaderBase {
    std::string_view getString(const int key) const override {
        static std::array<char, 256> buffer{};
        snprintf(buffer.data(), buffer.size(), "KEY: %d", key);
        return buffer.data();
    }
};

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    TgBot_AbslLogInit();
    CommandLine::initInstance(argc, argv);
    StringResManager::initInstance(std::make_unique<StringResLoaderFake>());
    int ret = RUN_ALL_TESTS();
    return ret;
}