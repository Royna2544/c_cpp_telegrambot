#include <optional>

#include "CommandModulesTest.hpp"

namespace {

class AskCmdTest : public CommandTestBase {
   public:
    AskCmdTest() : CommandTestBase("ask") {}
};

TEST_F(AskCmdTest, SchedulesEntireRequestOnLlmLaneWithoutRunningInline) {
    setCommandExtArgs();
    TgBotApi::CancellableWork scheduledWork;

    EXPECT_CALL(*botApi, submitCommandWork("ask", TgBotApi::WorkClass::Llm,
                                           testing::_, testing::_))
        .WillOnce(testing::DoAll(
            testing::SaveArg<2>(&scheduledWork),
            testing::Return(std::optional<TgBotApi::WorkId>{42})));

    execute();

    EXPECT_TRUE(static_cast<bool>(scheduledWork));
    // Destroy the type-erased closure while the dynamically loaded module is
    // still resident; its body intentionally remains unexecuted in this test.
    scheduledWork = {};
}

}  // namespace
