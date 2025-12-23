#include <absl/log/log.h>

#include <algorithm>
#include <filesystem>
#include <libfs.hpp>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <unordered_set>
#include <vector>

#include "../GetCommandLine.hpp"
#include "CommandModulesTest.hpp"

namespace {
const std::unordered_set<std::string_view> kExcludedModules = {"rombuild",
                                                               "kernelbuild"};

std::vector<std::string> discoverModules() {
    const auto moduleDir = getCmdLine().getPath(FS::PathType::CMD_MODULES);
    std::vector<std::string> names;

    std::error_code ec;
    if (!std::filesystem::exists(moduleDir, ec)) {
        LOG(WARNING) << "Module directory missing: " << moduleDir;
        return names;
    }

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
        const std::string_view prefix = DynCommandModule::prefix;
        const std::string_view stem_view{stem};
        if (!stem_view.starts_with(prefix)) continue;

        const auto name = stem_view.substr(prefix.size());
        if (kExcludedModules.contains(name)) continue;
        names.emplace_back(name);
    }

    std::sort(names.begin(), names.end());
    names.erase(std::unique(names.begin(), names.end()), names.end());
    return names;
}

const std::vector<std::string>& discoveredModules() {
    static const auto modules = discoverModules();
    return modules;
}
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
    ASSERT_NE(module, nullptr);
    const auto& info = module->info;

    EXPECT_FALSE(info.name.empty());
    ASSERT_TRUE(info.function);
    using HandlerCallable = CommandModule::command_callback_t;
    EXPECT_TRUE(
        (std::is_invocable_r_v<HandlerCallable, TgBotApi::Ptr, MessageExt*,
                               const StringResLoader::PerLocaleMap*,
                               const Providers*>))
        << "Handlers must match the DynModule callback signature.";
}

INSTANTIATE_TEST_SUITE_P(AllModules, AllCommandModulesSignatureTest,
                         ::testing::ValuesIn(discoveredModules()));

TEST(CommandModulesDiscoveryTest, HasModulesToValidate) {
    const auto& modules = discoveredModules();
    if (modules.empty()) {
        GTEST_SKIP() << "No command modules built for testing.";
    }
    SUCCEED();
}
