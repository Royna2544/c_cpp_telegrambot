#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <optional>
#include <string>

#include "CommandModulesTest.hpp"
#include "Env.hpp"

namespace {

class AskCmdTest : public CommandTestBase {
   public:
    AskCmdTest() : CommandTestBase("ask") {}

   protected:
    void runCancelledAsk(bool acceptTerminalReply) {
        static std::atomic_uint64_t nextDirectory{1};
        const auto root =
            std::filesystem::temp_directory_path() /
            ("glider-ask-test-" + std::to_string(nextDirectory.fetch_add(1)));
        ASSERT_TRUE(std::filesystem::create_directories(root / "v1"));
        {
            std::ofstream models(root / "v1" / "models");
            ASSERT_TRUE(models.good());
            models << R"({"data":[{"id":"test-model"}]})";
        }
        struct DirectoryCleanup {
            std::filesystem::path path;
            ~DirectoryCleanup() {
                std::error_code error;
                std::filesystem::remove_all(path, error);
            }
        } cleanup{root};

        auto genericRoot = std::filesystem::absolute(root).generic_string();
#ifdef _WIN32
        std::string modelUrl = "file:///" + genericRoot;
#else
        std::string modelUrl = "file://" + genericRoot;
#endif
        struct ScopedEnv {
            std::string key;
            std::optional<std::string> previous;
            ScopedEnv(std::string key, std::string_view value)
                : key(std::move(key)), previous(Env{}[this->key].get()) {
                Env{}[this->key] = value;
            }
            ~ScopedEnv() {
                if (previous) {
                    Env{}[key] = *previous;
                } else {
                    Env{}[key].clear();
                }
            }
        } urlEnv{"LLM.Url", modelUrl}, typeEnv{"LLM.ApiType", "OpenAI"};

        ON_CALL(strings, get(Strings::LLM_PROCESSING_QUERY))
            .WillByDefault(testing::Return("processing"));
        ON_CALL(strings, get(Strings::LLM_RESPONSE_FAILED))
            .WillByDefault(testing::Return("failed"));

        if (acceptTerminalReply) {
            {
                testing::InSequence outboundOrder;
                willSendReplyMessage("processing");
                willSendReplyMessage("failed");
            }
        } else {
            willSendReplyMessage("processing");
            EXPECT_CALL(
                *botApi,
                sendMessage_impl(
                    TEST_CHAT_ID, testing::Eq(std::string_view{"failed"}),
                    testing::_, testing::_, TgBotApi::ParseMode::None))
                .Times(0);
        }
        EXPECT_CALL(*botApi, editMessage_impl(testing::_, testing::_,
                                              testing::_, testing::_))
            .Times(0);

        std::stop_source cancellation;
        EXPECT_CALL(
            *database,
            checkUserInList(DatabaseBase::ListType::BLACKLIST, TEST_USER_ID))
            .WillOnce(testing::InvokeWithoutArgs([&] {
                cancellation.request_stop();
                return DatabaseBase::ListResult::NOT_IN_LIST;
            }));
        EXPECT_CALL(
            *database,
            checkUserInList(DatabaseBase::ListType::WHITELIST, TEST_USER_ID))
            .WillOnce(testing::Return(DatabaseBase::ListResult::NOT_IN_LIST));
        EXPECT_CALL(*database, getOwnerUserId())
            .WillOnce(testing::Return(std::nullopt));

        TgBotApi::CancellableWork scheduledWork;
        int outboundSubmissions = 0;
        EXPECT_CALL(*botApi, submitCommandWork("ask", testing::_, testing::_,
                                               testing::_))
            .Times(3)
            .WillRepeatedly(testing::Invoke(
                [&](std::string_view, TgBotApi::WorkClass workClass,
                    TgBotApi::CancellableWork work,
                    TgBotApi::WorkOptions options)
                    -> std::optional<TgBotApi::WorkId> {
                    if (workClass == TgBotApi::WorkClass::Llm) {
                        EXPECT_EQ(options.deadline, std::chrono::minutes(7));
                        scheduledWork = std::move(work);
                        return 42;
                    }
                    EXPECT_EQ(workClass, TgBotApi::WorkClass::Outbound);
                    ++outboundSubmissions;
                    if (outboundSubmissions == 1 || acceptTerminalReply) {
                        work(std::stop_token{});
                        return 42 + outboundSubmissions;
                    }
                    // Mirrors ModulesManagement::submitWork after
                    // stopExecutions(): the terminal-reply closure is rejected
                    // and destroyed here before the command DSO can unload.
                    return std::nullopt;
                }));

        setCommandExtArgs({" query"});
        execute();
        ASSERT_TRUE(static_cast<bool>(scheduledWork));
        auto completed = std::async(std::launch::async, [&] {
            scheduledWork(cancellation.get_token());
        });
        ASSERT_EQ(completed.wait_for(std::chrono::seconds(5)),
                  std::future_status::ready);
        completed.get();
        EXPECT_EQ(outboundSubmissions, 2);
        scheduledWork = {};
    }
};

TEST_F(AskCmdTest, SchedulesEntireRequestOnLlmLaneWithoutRunningInline) {
    setCommandExtArgs();
    TgBotApi::CancellableWork scheduledWork;
    TgBotApi::WorkOptions scheduledOptions;

    EXPECT_CALL(*botApi, submitCommandWork("ask", TgBotApi::WorkClass::Llm,
                                           testing::_, testing::_))
        .WillOnce(testing::DoAll(
            testing::SaveArg<2>(&scheduledWork),
            testing::SaveArg<3>(&scheduledOptions),
            testing::Return(std::optional<TgBotApi::WorkId>{42})));

    execute();

    EXPECT_TRUE(static_cast<bool>(scheduledWork));
    EXPECT_EQ(scheduledOptions.deadline, std::chrono::minutes(7));
    // Destroy the type-erased closure while the dynamically loaded module is
    // still resident; its body intentionally remains unexecuted in this test.
    scheduledWork = {};
}

TEST_F(AskCmdTest, LiteralResponseBypassesModelAndProcessing) {
    struct ScopedEnv {
        std::string key;
        std::optional<std::string> previous;
        ScopedEnv(std::string key, std::string_view value)
            : key(std::move(key)), previous(Env{}[this->key].get()) {
            Env{}[this->key] = value;
        }
        ~ScopedEnv() {
            if (previous) {
                Env{}[key] = *previous;
            } else {
                Env{}[key].clear();
            }
        }
    } urlEnv{"LLM.Url", "file:///this-path-must-not-be-read"},
        typeEnv{"LLM.ApiType", "OpenAI"};

    EXPECT_CALL(*botApi, submitCommandWork("ask", TgBotApi::WorkClass::Llm,
                                           testing::_, testing::_))
        .WillOnce(testing::Invoke([](std::string_view, TgBotApi::WorkClass,
                                     TgBotApi::CancellableWork work,
                                     TgBotApi::WorkOptions options)
                                      -> std::optional<TgBotApi::WorkId> {
            EXPECT_EQ(options.deadline, std::chrono::minutes(7));
            work(std::stop_token{});
            return 42;
        }));
    EXPECT_CALL(*botApi, submitCommandWork("ask", TgBotApi::WorkClass::Outbound,
                                           testing::_, testing::_))
        .WillOnce(testing::Invoke(
            [](std::string_view, TgBotApi::WorkClass,
               TgBotApi::CancellableWork work,
               TgBotApi::WorkOptions) -> std::optional<TgBotApi::WorkId> {
                work(std::stop_token{});
                return 43;
            }));
    willSendReplyMessage("FINAL FAST PATH OK");

    setCommandExtArgs({" Say exactly FINAL FAST PATH OK"});
    execute();
}

TEST_F(AskCmdTest, DeadlineCancellationQueuesTerminalFailureAfterProcessing) {
    runCancelledAsk(true);
}

TEST_F(AskCmdTest, UnloadCancellationDoesNotRetainRejectedTerminalReply) {
    runCancelledAsk(false);
}

}  // namespace
