#include "CommandModulesTest.hpp"
#include <StringResManager.hpp>

using testing::HasSubstr;

struct CMDCommandTest : public CommandTestBase {
    CMDCommandTest() : CommandTestBase("cmd") {}
    
    void makeReload(const bool ret) {
        EXPECT_CALL(*botApi, reloadCommand(testCmd)).WillOnce(Return(ret));
    }
    void makeUnload(const bool ret) {
        EXPECT_CALL(*botApi, unloadCommand(testCmd)).WillOnce(Return(ret));
    }

    const std::string testCmd = "testingcmd";
    const std::string failureMessage = GETSTR(OPERATION_FAILURE);
    const std::string successMessage = GETSTR(OPERATION_SUCCESSFUL);
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