#include "CommandModulesTest.hpp"

using testing::_;

class DelayCommandTest : public CommandTestBase {
   public:
    DelayCommandTest() : CommandTestBase("delay") {}
    ~DelayCommandTest() override = default;
};

TEST_F(DelayCommandTest, SendsDelayInformation) {
    setCommandExtArgs();
    
    // Expect first reply message with initial timing info
    auto sentMsg = willSendReplyMessage(_);
    
    // Expect edit message with final timing info including send delay
    sentMsg.willEdit(_);
    
    execute();
}
