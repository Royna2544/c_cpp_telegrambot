#include <chrono>
#include <future>
#include <optional>
#include <string>
#include <thread>

#include "CommandModulesTest.hpp"

using namespace std::chrono_literals;

struct FlashCommandTest : CommandTestBase {
    FlashCommandTest() : CommandTestBase("flash") {}
};

TEST_F(FlashCommandTest, SchedulesDelayWithoutSleepingFastWorker) {
    setCommandExtArgs({"update"});
    ON_CALL(*resource, get(testing::_))
        .WillByDefault(testing::Return("bad cable\nwrong slot"));
    EXPECT_CALL(*random, generate(0, 5)).WillOnce(testing::Return(5));
    EXPECT_CALL(*random, generate(0, 1)).WillOnce(testing::Return(0));
    EXPECT_CALL(*random, generate(0, 999)).WillOnce(testing::Return(999));
    ON_CALL(strings, get(Strings::FLASHING_ZIP))
        .WillByDefault(testing::Return("Flashing"));
    ON_CALL(strings, get(Strings::FAILED_SUCCESSFULLY))
        .WillByDefault(testing::Return("Failed"));
    ON_CALL(strings, get(Strings::REASON))
        .WillByDefault(testing::Return("Reason"));

    const auto sent = willSendReplyMessage(testing::HasSubstr("update.zip"));
    EXPECT_CALL(*botApi,
                editMessage_impl(sent.message, testing::HasSubstr("bad cable"),
                                 testing::IsNull(), TgBotApi::ParseMode::None))
        .WillOnce(testing::Return(sent.message));
    EXPECT_CALL(*botApi,
                submitCommandWork("flash", testing::_, testing::_, testing::_))
        .Times(2);

    const auto started = std::chrono::steady_clock::now();
    execute();
    EXPECT_LT(std::chrono::steady_clock::now() - started, 100ms);
}

struct SpamCommandTest : CommandTestBase {
    SpamCommandTest() : CommandTestBase("spam") {}
};

TEST_F(SpamCommandTest, SchedulesEachSendWithoutWorkerSleeps) {
    setCommandExtArgs({"3 hello"});
    EXPECT_CALL(*botApi,
                submitCommandWork("spam", TgBotApi::WorkClass::Outbound,
                                  testing::_, testing::_))
        .Times(3);
    EXPECT_CALL(*botApi,
                sendMessage_impl(TEST_CHAT_ID, "hello", testing::IsNull(),
                                 testing::IsNull(), TgBotApi::ParseMode::None))
        .Times(3)
        .WillRepeatedly(testing::Return(createDefaultMessage()));

    const auto started = std::chrono::steady_clock::now();
    execute();
    EXPECT_LT(std::chrono::steady_clock::now() - started, 100ms);
}

struct BashCommandTest : CommandTestBase {
    BashCommandTest() : CommandTestBase("bash") {}
};

TEST_F(BashCommandTest, RoutesExecutionToBoundedProcessLane) {
    setCommandExtArgs({"echo scheduled"});
    TgBotApi::CancellableWork scheduled;
    EXPECT_CALL(*botApi, submitCommandWork("bash", TgBotApi::WorkClass::Process,
                                           testing::_, testing::_))
        .WillOnce(testing::DoAll(
            testing::SaveArg<2>(&scheduled),
            testing::Return(std::optional<TgBotApi::WorkId>{41})));

    execute();

    EXPECT_TRUE(static_cast<bool>(scheduled));
}

struct UbashCommandTest : CommandTestBase {
    UbashCommandTest() : CommandTestBase("ubash") {}
};

TEST_F(UbashCommandTest, CancelRequestsStopForTheActiveUnlimitedJob) {
    setCommandExtArgs({"long-running-command"});
    TgBotApi::CancellableWork scheduled;
    EXPECT_CALL(*botApi, submitCommandWork(
                             "ubash", TgBotApi::WorkClass::UnboundedProcess,
                             testing::_, testing::_))
        .WillOnce(testing::DoAll(
            testing::SaveArg<2>(&scheduled),
            testing::Return(std::optional<TgBotApi::WorkId>{42})));
    execute();
    ASSERT_TRUE(static_cast<bool>(scheduled));

    setCommandExtArgs({"cancel   "});
    EXPECT_CALL(*botApi, cancelCommandWork("ubash", 42))
        .WillOnce(testing::Return(true));
    willSendReplyMessage("Cancellation requested.");
    execute();

    // A queued job may be removed without ever invoking its callback, so the
    // command module must release its own active-job bookkeeping on cancel.
    setCommandExtArgs({"next-command"});
    EXPECT_CALL(*botApi, submitCommandWork(
                             "ubash", TgBotApi::WorkClass::UnboundedProcess,
                             testing::_, testing::_))
        .WillOnce(testing::Return(std::optional<TgBotApi::WorkId>{43}));
    execute();
}

struct CCommandTest : CommandTestBase {
    CCommandTest() : CommandTestBase("c") {}
};

TEST_F(CCommandTest, RoutesExecutionToBoundedProcessLane) {
    setCommandExtArgs();
    EXPECT_CALL(*botApi, submitCommandWork("c", TgBotApi::WorkClass::Process,
                                           testing::_, testing::_))
        .WillOnce(testing::Return(std::optional<TgBotApi::WorkId>{43}));
    execute();
}

struct CppCommandTest : CommandTestBase {
    CppCommandTest() : CommandTestBase("cpp") {}
};

TEST_F(CppCommandTest, RoutesExecutionToBoundedProcessLane) {
    setCommandExtArgs();
    EXPECT_CALL(*botApi, submitCommandWork("cpp", TgBotApi::WorkClass::Process,
                                           testing::_, testing::_))
        .WillOnce(testing::Return(std::optional<TgBotApi::WorkId>{44}));
    execute();
}

#ifdef TEST_HAVE_CMD_PY
struct PyCommandTest : CommandTestBase {
    PyCommandTest() : CommandTestBase("py") {}
};

TEST_F(PyCommandTest, RoutesExecutionToBoundedProcessLane) {
    setCommandExtArgs();
    EXPECT_CALL(*botApi, submitCommandWork("py", TgBotApi::WorkClass::Process,
                                           testing::_, testing::_))
        .WillOnce(testing::Return(std::optional<TgBotApi::WorkId>{45}));
    execute();
}
#endif

#ifdef TEST_HAVE_CMD_IBASH
struct IbashCommandTest : CommandTestBase {
    IbashCommandTest() : CommandTestBase("ibash") {}
};

TEST_F(IbashCommandTest, RoutesInteractiveIoToBoundedProcessLane) {
    setCommandExtArgs({"printf test"});
    TgBotApi::CancellableWork scheduled;
    EXPECT_CALL(*botApi,
                submitCommandWork("ibash", TgBotApi::WorkClass::Process,
                                  testing::_, testing::_))
        .WillOnce(testing::DoAll(
            testing::SaveArg<2>(&scheduled),
            testing::Return(std::optional<TgBotApi::WorkId>{46})));

    execute();

    EXPECT_TRUE(static_cast<bool>(scheduled));
}

TEST_F(IbashCommandTest, ExactCancelDoesNotEnterTheProcessLane) {
    setCommandExtArgs({"cancel"});
    ON_CALL(strings, get(Strings::IBASH_NO_ACTIVE_SESSION))
        .WillByDefault(testing::Return("no active session"));
    EXPECT_CALL(*botApi, submitCommandWork(testing::_, testing::_, testing::_,
                                           testing::_))
        .Times(0);
    willSendReplyMessage("no active session");

    execute();
}

TEST_F(IbashCommandTest, TrimmedCancelKeepsCommandTextAlive) {
    setCommandExtArgs({"  cancel  "});
    ON_CALL(strings, get(Strings::IBASH_NO_ACTIVE_SESSION))
        .WillByDefault(testing::Return("no active session"));
    EXPECT_CALL(*botApi, submitCommandWork(testing::_, testing::_, testing::_,
                                           testing::_))
        .Times(0);
    willSendReplyMessage("no active session");

    execute();
}

TEST_F(IbashCommandTest, SessionsAreScopedByChatAndUser) {
    ON_CALL(strings, get(Strings::IBASH_SESSION_STARTED))
        .WillByDefault(testing::Return("session started"));
    ON_CALL(strings, get(Strings::IBASH_START_FIRST))
        .WillByDefault(testing::Return("start first"));

    setCommandExtArgs();
    TgBotApi::CancellableWork start;
    EXPECT_CALL(*botApi,
                submitCommandWork("ibash", TgBotApi::WorkClass::Process,
                                  testing::_, testing::_))
        .WillOnce(testing::DoAll(
            testing::SaveArg<2>(&start),
            testing::Return(std::optional<TgBotApi::WorkId>{47})));
    willSendReplyMessage("session started");
    execute();
    ASSERT_TRUE(static_cast<bool>(start));
    start(std::stop_token{});

    defaultProvidedMessage->from = createDefaultUser(1);
    setCommandExtArgs({"printf isolated"});
    TgBotApi::CancellableWork other_user;
    EXPECT_CALL(*botApi,
                submitCommandWork("ibash", TgBotApi::WorkClass::Process,
                                  testing::_, testing::_))
        .WillOnce(testing::DoAll(
            testing::SaveArg<2>(&other_user),
            testing::Return(std::optional<TgBotApi::WorkId>{48})));
    willSendReplyMessage("start first");
    execute();
    ASSERT_TRUE(static_cast<bool>(other_user));
    other_user(std::stop_token{});
}

TEST_F(IbashCommandTest, CancelInterruptsRunningCommandWithoutWorkerHang) {
    ON_CALL(strings, get(Strings::IBASH_SESSION_STARTED))
        .WillByDefault(testing::Return("session started"));
    ON_CALL(strings, get(Strings::IBASH_NO_OUTPUT))
        .WillByDefault(testing::Return("no output"));
    ON_CALL(strings, get(Strings::IBASH_OUTPUT_TRUNCATED))
        .WillByDefault(testing::Return("truncated"));
    ON_CALL(strings, get(Strings::IBASH_EXEC_FAILED))
        .WillByDefault(testing::Return("execution failed"));
    ON_CALL(strings, get(Strings::IBASH_SESSION_ENDED))
        .WillByDefault(testing::Return("session ended"));

    setCommandExtArgs();
    TgBotApi::CancellableWork start;
    EXPECT_CALL(*botApi,
                submitCommandWork("ibash", TgBotApi::WorkClass::Process,
                                  testing::_, testing::_))
        .WillOnce(testing::DoAll(
            testing::SaveArg<2>(&start),
            testing::Return(std::optional<TgBotApi::WorkId>{49})));
    willSendReplyMessage("session started");
    execute();
    ASSERT_TRUE(static_cast<bool>(start));
    start(std::stop_token{});

    setCommandExtArgs({"sleep 30"});
    TgBotApi::CancellableWork long_command;
    EXPECT_CALL(*botApi,
                submitCommandWork("ibash", TgBotApi::WorkClass::Process,
                                  testing::_, testing::_))
        .WillOnce(testing::DoAll(
            testing::SaveArg<2>(&long_command),
            testing::Return(std::optional<TgBotApi::WorkId>{50})));
    execute();
    ASSERT_TRUE(static_cast<bool>(long_command));

    willSendReplyMessage("execution failed");
    auto running = std::async(std::launch::async, [long_command] {
        long_command(std::stop_token{});
    });
    std::this_thread::sleep_for(100ms);

    setCommandExtArgs({"cancel"});
    willSendReplyMessage("session ended");
    execute();

    EXPECT_EQ(running.wait_for(3s), std::future_status::ready);
}
#endif
