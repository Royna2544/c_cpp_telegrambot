#include "CommandModulesTest.hpp"

using testing::_;
using testing::HasSubstr;

class FlashCommandTest : public CommandTestBase {
   public:
    FlashCommandTest() : CommandTestBase("flash") {
        ON_CALL(strings, get(Strings::SEND_A_FILE_NAME_TO_FLASH))
            .WillByDefault(Return("Send a file name"));
        ON_CALL(strings, get(Strings::INVALID_INPUT_NO_NEWLINE))
            .WillByDefault(Return("Invalid input no newline"));
        ON_CALL(strings, get(Strings::FLASHING_ZIP))
            .WillByDefault(Return("Flashing"));
        ON_CALL(strings, get(Strings::FAILED_SUCCESSFULLY))
            .WillByDefault(Return("Failed successfully"));
        ON_CALL(strings, get(Strings::REASON))
            .WillByDefault(Return("Reason"));
        ON_CALL(strings, get(Strings::SUCCESS_CHANCE_WAS))
            .WillByDefault(Return("Success chance was"));
    }
    ~FlashCommandTest() override = default;
};

TEST_F(FlashCommandTest, NoFileName) {
    setCommandExtArgs();
    
    willSendReplyMessage(HasSubstr("Send a file name"));
    execute();
}

TEST_F(FlashCommandTest, FileNameWithNewline) {
    setCommandExtArgs({"file\nname"});
    
    willSendReplyMessage(HasSubstr("Invalid input no newline"));
    execute();
}

TEST_F(FlashCommandTest, ValidFileName) {
    setCommandExtArgs({"myfile"});
    
    // Mock resource provider for flash.txt
    ON_CALL(*resource, get("flash.txt"))
        .WillByDefault(Return("reason1\nreason2\nreason3"));
    
    // Mock random for delay and reason selection (two parameters: min, max)
    ON_CALL(*random, generate(_, _))
        .WillByDefault(Return(1));
    
    auto sentMsg = willSendReplyMessage(HasSubstr("Flashing"));
    sentMsg.willEdit(_);
    
    execute();
}
