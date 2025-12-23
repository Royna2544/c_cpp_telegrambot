#include "CommandModulesTest.hpp"

using testing::_;
using testing::HasSubstr;

class DecideCommandTest : public CommandTestBase {
   public:
    DecideCommandTest() : CommandTestBase("decide") {
        ON_CALL(strings, get(Strings::DECIDING))
            .WillByDefault(Return("Deciding"));
        ON_CALL(strings, get(Strings::YES))
            .WillByDefault(Return("Yes"));
        ON_CALL(strings, get(Strings::NO))
            .WillByDefault(Return("No"));
        ON_CALL(strings, get(Strings::SO_YES))
            .WillByDefault(Return("So yes"));
        ON_CALL(strings, get(Strings::SO_NO))
            .WillByDefault(Return("So no"));
        ON_CALL(strings, get(Strings::SO_IDK))
            .WillByDefault(Return("So idk"));
    }
    ~DecideCommandTest() override = default;
};

TEST_F(DecideCommandTest, DecisionMade) {
    setCommandExtArgs({"something"});
    
    // Expect initial reply message
    auto sentMsg = willSendReplyMessage(HasSubstr("Deciding"));
    
    // Expect multiple edit calls as decision is made
    EXPECT_CALL(*botApi, editMessage_impl(_, _, _, _))
        .Times(testing::AtLeast(1));
    
    // Expect setMessageReaction to be called with any reaction
    EXPECT_CALL(*botApi, setMessageReaction_impl(_, _, _))
        .Times(1);
    
    // Mock random number generation (two parameters: min, max)
    ON_CALL(*random, generate(_, _))
        .WillByDefault(Return(5));
    
    execute();
}
