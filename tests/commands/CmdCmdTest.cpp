#include "CommandModulesTest.hpp"

using testing::HasSubstr;

struct CMDCommandTest : public CommandTestBase {
    CMDCommandTest() : CommandTestBase("cmd") {
        ON_CALL(strings, get(Strings::OPERATION_SUCCESSFUL))
            .WillByDefault(Return("SSUCCESSFUL"));
        ON_CALL(strings, get(Strings::OPERATION_FAILURE))
            .WillByDefault(Return("FAILURE"));
    }

    void makeReload(const bool ret) {
        EXPECT_CALL(*botApi, reloadCommand(testCmd.data()))
            .WillOnce(Return(ret));
    }
    void makeUnload(const bool ret) {
        EXPECT_CALL(*botApi, unloadCommand(testCmd.data()))
            .WillOnce(Return(ret));
    }

    constexpr static std::string_view testCmd = "testingcmd";
    constexpr static std::string_view failureMessage = "FAILURE";
    constexpr static std::string_view successMessage = "SSUCCESSFUL";
};

TEST_F(CMDCommandTest, LoadCommandFail) {
    setCommandExtArgs({testCmd, "reload"});
    makeReload(false);
    willSendReplyMessage(HasSubstr(failureMessage));
    execute();
}

TEST_F(CMDCommandTest, LoadCommandSuccess) {
    setCommandExtArgs({testCmd, "reload"});
    makeReload(true);
    willSendReplyMessage(HasSubstr(successMessage));
    execute();
}

TEST_F(CMDCommandTest, UnloadCommandFail) {
    setCommandExtArgs({testCmd, "unload"});
    makeUnload(false);
    willSendReplyMessage(HasSubstr(failureMessage));
    execute();
}

TEST_F(CMDCommandTest, UnloadCommandSuccess) {
    setCommandExtArgs({testCmd, "unload"});
    makeUnload(true);
    willSendReplyMessage(HasSubstr(successMessage));
    execute();
}