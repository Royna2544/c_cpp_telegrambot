#include "CommandModulesTest.hpp"
#include <StringResManager.hpp>

struct CMDCommandTest : public CommandTestBase {
    CMDCommandTest() : CommandTestBase("cmd") {}
    
    void makeReload(const bool ret) {
        EXPECT_CALL(*botApi, reloadCommand(testCmd)).WillOnce(Return(ret));
    }
    void makeUnload(const bool ret) {
        EXPECT_CALL(*botApi, unloadCommand(testCmd)).WillOnce(Return(ret));
    }

    const std::string testCmd = "testingcmd";
    const std::string failureMessage = GETSTR_IS(OPERATION_FAILURE) + testCmd;
    const std::string successMessage = GETSTR_IS(OPERATION_SUCCESSFUL) + testCmd;
};

TEST_F(CMDCommandTest, UnknownCommand) {
    setCommandExtArgs("pwd");
    willSendReplyMessage(_);
    execute();
}

TEST_F(CMDCommandTest, LoadCommandFail) {
    setCommandExtArgs("testingcmd reload");
    makeReload(false);
    willSendReplyMessage(failureMessage);
    execute();
}

TEST_F(CMDCommandTest, LoadCommandSuccess) {
    setCommandExtArgs("testingcmd reload");
    makeReload(true);
    willSendReplyMessage(successMessage);
    execute();
}

TEST_F(CMDCommandTest, UnloadCommandFail) {
    setCommandExtArgs("testingcmd unload");
    makeUnload(false);
    willSendReplyMessage(failureMessage);
    execute();
}

TEST_F(CMDCommandTest, UnloadCommandSuccess) {
    setCommandExtArgs("testingcmd unload");
    makeUnload(true);
    willSendReplyMessage(successMessage);
    execute();
}