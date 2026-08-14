#include <gtest/gtest.h>

#include <sstream>
#include <string>

#include "compiler/CompilerInTelegram.hpp"

namespace {

class TestLocale final : public StringResLoader::PerLocaleMap {
   public:
    std::string_view get(Strings key) const override {
        if (key == Strings::IBASH_OUTPUT_TRUNCATED) {
            return "[Output truncated]";
        }
        return "test";
    }
};

class RecordingCallback final : public CompilerInTg::Interface {
   public:
    void onExecutionStarted(const std::string_view&) override {}
    void onExecutionFinished(const std::string_view&,
                             const popen_watchdog_exit_t& status) override {
        exit = status;
    }
    void onErrorStatus(TinyStatus) override { sawError = true; }
    void onResultReady(const std::string&) override {}
    void onWdtTimeout() override {}

    popen_watchdog_exit_t exit = POPEN_WDT_EXIT_INITIALIZER;
    bool sawError = false;
};

class TestCompiler final : public CompilerInTg {
   public:
    using CompilerInTg::CompilerInTg;
    void run(MessageExt::Ptr) override {}
};

}  // namespace

#ifndef _WIN32
TEST(CompilerBaseTest, DrainsButCapsProcessOutputExactly) {
    TestLocale locale;
    auto callback = std::make_unique<RecordingCallback>();
    auto* callbackView = callback.get();
    TestCompiler compiler(std::move(callback), &locale);
    std::stringstream output;

    compiler.runCommand("yes x | head -c 10000", output, false);

    const auto text = output.str();
    EXPECT_FALSE(callbackView->sawError);
    EXPECT_EQ(callbackView->exit.exitcode, 0);
    EXPECT_EQ(text.size(), static_cast<std::size_t>(BASH_MAX_BUF));
    EXPECT_TRUE(text.ends_with("\n[Output truncated]"));
}
#endif
