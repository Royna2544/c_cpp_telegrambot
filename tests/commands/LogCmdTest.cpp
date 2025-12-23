#include "CommandModulesTest.hpp"

using testing::_;

class LogCommandTest : public CommandTestBase {
   public:
    LogCommandTest() : CommandTestBase("log") {}
    ~LogCommandTest() override = default;
};

TEST_F(LogCommandTest, SendsLogFile) {
    setCommandExtArgs();
    
    // Expect sendDocument to be called with all 6 parameters
    EXPECT_CALL(*botApi, sendDocument_impl(TEST_CHAT_ID, _, _, _, _, _));
    execute();
}
