#include "CommandModulesTest.hpp"

using testing::_;
using testing::HasSubstr;
using testing::Return;

class PossibilityCommandTest : public CommandTestBase {
   public:
    PossibilityCommandTest() : CommandTestBase("possibility") {
        ON_CALL(strings, get(Strings::SEND_POSSIBILITIES))
            .WillByDefault(Return("Send possibilities"));
        ON_CALL(strings, get(Strings::GIVE_MORE_THAN_ONE))
            .WillByDefault(Return("Give more than one"));
        ON_CALL(strings, get(Strings::TOTAL_ITEMS_PREFIX))
            .WillByDefault(Return("Total items:"));
        ON_CALL(strings, get(Strings::TOTAL_ITEMS_SUFFIX))
            .WillByDefault(Return("items"));
    }
    ~PossibilityCommandTest() override = default;
};

TEST_F(PossibilityCommandTest, NoArguments) {
    setCommandExtArgs();
    
    willSendReplyMessage(HasSubstr("Send possibilities"));
    execute();
}

TEST_F(PossibilityCommandTest, OneItem) {
    setCommandExtArgs({"item1"});
    
    willSendReplyMessage(HasSubstr("Give more than one"));
    execute();
}

TEST_F(PossibilityCommandTest, MultipleItems) {
    // Set message with newline-separated items
    setCommandExtArgs();
    defaultProvidedMessage->text = "/possibility item1\nitem2\nitem3";
    
    // Mock random generation (two parameters: min, max)
    ON_CALL(*random, generate(_, _))
        .WillByDefault(Return(25));
    EXPECT_CALL(*random, shuffle(_));
    
    willSendReplyMessage(HasSubstr("Total items:"));
    execute();
}
