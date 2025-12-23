#include <absl/log/log.h>

#include <algorithm>
#include <filesystem>
#include <libfs.hpp>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "../GetCommandLine.hpp"
#include "CommandModulesTest.hpp"

namespace {
std::vector<std::string> discoverModules() {
    const auto moduleDir = getCmdLine().getPath(FS::PathType::CMD_MODULES);
    std::vector<std::string> names;

    std::error_code ec;
    if (!std::filesystem::exists(moduleDir, ec)) {
        LOG(WARNING) << "Module directory missing: " << moduleDir;
        return names;
    }

    const std::unordered_set<std::string_view> excluded = {"rombuild",
                                                           "kernelbuild"};

    for (const auto& entry :
         std::filesystem::directory_iterator(moduleDir, ec)) {
        if (ec) {
            LOG(ERROR) << "Iterating " << moduleDir
                       << " failed: " << ec.message();
            break;
        }
        if (!entry.is_regular_file()) continue;
        const auto& path = entry.path();
        if (path.extension() != FS::kDylibExtension) continue;

        const auto stem = path.stem().string();
        if (!stem.starts_with(std::string(DynCommandModule::prefix))) continue;

        const auto name = stem.substr(DynCommandModule::prefix.size());
        if (excluded.contains(name)) continue;
        names.push_back(name);
    }

    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}

const auto kDiscoveredModules = discoverModules();
}  // namespace

class AllCommandModulesSignatureTest
    : public CommandModulesTest,
      public ::testing::WithParamInterface<std::string> {
   public:
    void SetUp() override {
        CommandModulesTest::SetUp();
        module = loadModule(GetParam());
    }

    void TearDown() override {
        if (module) {
            unloadModule(std::move(module));
        }
        CommandModulesTest::TearDown();
    }

   protected:
    CommandModule::Ptr module;
};

TEST_P(AllCommandModulesSignatureTest, HasCallableHandler) {
    if (kDiscoveredModules.empty()) {
        GTEST_SKIP() << "No command modules built for testing.";
    }
    ASSERT_NE(module, nullptr);
    const auto& info = module->info;

    EXPECT_FALSE(info.name.empty());
    EXPECT_TRUE(info.function);
    EXPECT_NE(info.function.target<DynModule::command_callback_t>(),
              nullptr);  // Ensure signature matches the expected handler type
}

INSTANTIATE_TEST_SUITE_P(AllModules, AllCommandModulesSignatureTest,
                         ::testing::ValuesIn(kDiscoveredModules));
