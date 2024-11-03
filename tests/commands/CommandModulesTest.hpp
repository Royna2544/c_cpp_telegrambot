#include <fmt/format.h>
#include <fmt/ranges.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>
#include <database/DatabaseBase.hpp>
#include <initializer_list>
#include <memory>
#include <source_location>
#include <utility>

#include "../ClassProviders.hpp"
#include "ConfigManager.hpp"
#include "api/Providers.hpp"
#include "api/Utils.hpp"
#include "fruit/fruit.h"

using testing::_;
using testing::DoAll;
using testing::EndsWith;
using testing::Invoke;
using testing::InvokeArgument;
using testing::IsNull;
using testing::Mock;
using testing::Return;
using testing::ReturnArg;
using testing::SaveArg;
using testing::StartsWith;
using testing::Truly;
using testing::WithArg;
using TgBot::ChatPermissions;

class CommandModulesTest : public ::testing::Test {
   public:
    CommandModulesTest() : provideInject(getProviders) {}

   protected:
    /**
     * @brief Set up the test fixture before each test case.
     * This function is called before each test case in the test fixture.
     * It is responsible for setting up any necessary test fixture state.
     */
    void SetUp() override;

    /**
     * @brief Tear down the test fixture after each test case.
     * This function is called after each test case in the test fixture.
     * It is responsible for cleaning up any necessary test fixture state.
     */
    void TearDown() override;

    /**
     * @brief Load a command module from the specified file.
     *
     * @param name The name of the file containing the command module.
     * @return loaded module handle if successful.
     */
    [[nodiscard]] CommandModule::Ptr loadModule(const std::string& name) const;

    /**
     * @brief Unload a command module.
     *
     * @param module The module handle to unload.
     */
    static void unloadModule(CommandModule::Ptr module);

    /**
     * @brief Create a default message object for testing purposes.
     *
     * @return A shared pointer to a default message object.
     */
    static Message::Ptr createDefaultMessage();

    /**
     * @brief Create a default user object for testing purposes.
     *
     * @param id_offset An optional offset to add to the default user ID.
     * @return A shared pointer to a default user object.
     */
    static User::Ptr createDefaultUser(long id_offset = 0);

    /**
     * @brief Check if a reply parameters object refers to a specific message.
     *
     * @param rhs The reply parameters object to check.
     * @param message The message to compare against.
     * @return True if the reply parameters refer to the specified message,
     *         false otherwise.
     */
    static bool isReplyToThisMsg(const ReplyParametersExt::Ptr& rhs,
                                 const Message::Ptr& message);

    /**
     * @brief Get the current working directory path.
     *
     * @return The current working directory path as a string.
     */
    static std::string current_path();

    static fruit::Component<MockTgBotApi, Providers, MockDatabase, MockResource,
                            CommandLine, MockRandom>
    getProviders();

    // Testing data
    static constexpr ChatId TEST_CHAT_ID = 123123;
    static constexpr UserId TEST_USER_ID = 456456;
    static constexpr MessageId TEST_MESSAGE_ID = 789789;
    static constexpr const char* TEST_USERNAME = "testuser";
    static constexpr const char* TEST_NICKNAME = "testnickname";
    static constexpr const char* ALIVE_FILE_ID = "alive";
    static constexpr const char* TEST_MEDIA_ID = "TOO_ID";
    static constexpr const char* TEST_MEDIA_UNIQUEID = "TOO_UNIQUE";
    static constexpr long TEST_BOT_USER_ID_OFFSET = 1000;
    const MediaIds TEST_MEDIAIDS{TEST_MEDIA_ID, TEST_MEDIA_UNIQUEID};
    const DatabaseBase::MediaInfo TEST_MEDIAINFO{TEST_MEDIA_ID,
                                                 TEST_MEDIA_UNIQUEID,
                                                 {"name1", "name2"},
                                                 DatabaseBase::MediaType::GIF};

    std::filesystem::path modulePath;
    // Mocked objects
    MockTgBotApi* botApi{};
    MockDatabase* database{};
    MockResource* resource{};
    MockRandom* random{};
    ConfigManager* configManager{};
    fruit::Injector<MockTgBotApi, Providers, MockDatabase, MockResource,
                    CommandLine, MockRandom>
        provideInject;
};

class CommandTestBase : public CommandModulesTest {
   protected:
    void SetUp() override {
        CommandModulesTest::SetUp();
        module = loadModule(name);
        ASSERT_NE(module, nullptr);
        defaultProvidedMessage = createDefaultMessage();
    }
    void TearDown() override {
        if (module) {
            unloadModule(std::move(module));
        }
        CommandModulesTest::TearDown();
    }

    constexpr static bool kTestDebug = true;
    using st = std::source_location;
    static void testRun(const std::string& message, const st& location) {
        if (!kTestDebug) {
            return;
        }
        std::cout << fmt::format("{} [file: {}, line: {}]", message,
                                 location.file_name(), location.line())
                  << std::endl;
    }

   public:
    void setCommandExtArgs(
        const std::initializer_list<std::string_view>& command) {
        setCommandExtArgs();
        defaultProvidedMessage->text +=
            fmt::format("{}", fmt::join(command, " "));
    }
    void setCommandExtArgs() {
        defaultProvidedMessage->text = "/" + name;
        defaultProvidedMessage->entities[0]->length =
            defaultProvidedMessage->text.size();
    }
    void execute() {
        module->function(
            botApi,
            std::make_shared<MessageExt>(defaultProvidedMessage,
                                         SplitMessageText::ByWhitespace),
            &strings, provideInject.get<Providers*>());
    }

    struct SentMessage {
        Message::Ptr message;
        MockTgBotApi* botApi;

        template <typename T>
        const SentMessage& willEdit(T&& matcher,
                                    const st& location = st::current()) const {
            testRun("Edit message expectation", location);
            EXPECT_CALL(
                *botApi,
                editMessage_impl(
                    _, matcher, IsNull(),
                    TgBotApi::parseModeToStr<TgBotApi::ParseMode::None>()))
                .WillOnce(DoAll(
                    WithArg<0>([=, addr = message.get()](auto&& message_in) {
                        EXPECT_EQ(message_in.get(), addr);
                    }),
                    ReturnArg<0>()));
            return *this;
        }
    };

    auto createMessageReplyMatcher(Message::Ptr message = nullptr) {
        return Truly([=, message = std::move(message),
                      this](ReplyParametersExt::Ptr m) {
            return isReplyToThisMsg(m, message
                                           ?: std::static_pointer_cast<Message>(
                                                  defaultProvidedMessage));
        });
    }

   private:
    // Template function for sending a message with expectations
    template <TgBotApi::ParseMode mode = TgBotApi::ParseMode::None,
              typename TextMatcher, typename ReplyMatcher, typename MarkMatcher>
    SentMessage _willSendMessage(TextMatcher&& textMatcher,
                                 ReplyMatcher&& replyMatcher,
                                 MarkMatcher&& markMatcher,
                                 const st& location) {
        const auto sentMessage = createDefaultMessage();
        sentMessage->from = createDefaultUser(TEST_BOT_USER_ID_OFFSET);
        testRun("Send message expectation", location);

        EXPECT_CALL(*botApi,
                    sendMessage_impl(TEST_CHAT_ID,
                                     std::forward<TextMatcher>(textMatcher),
                                     std::forward<ReplyMatcher>(replyMatcher),
                                     std::forward<MarkMatcher>(markMatcher),
                                     TgBotApi::parseModeToStr<mode>()))
            .WillOnce(Return(sentMessage));
        return {sentMessage, botApi};
    }
    template <TgBotApi::ParseMode mode = TgBotApi::ParseMode::None,
              typename FileIdMatcher, typename CaptionMatcher,
              typename ReplyMatcher, typename MarkMatcher = decltype(IsNull())>
    SentMessage _willSendFile(FileIdMatcher&& fileIdMatcher,
                              CaptionMatcher&& textMatcher,
                              ReplyMatcher&& replyMatcher,
                              MarkMatcher&& markMatcher, const st& location) {
        const auto sentMessage = createDefaultMessage();
        sentMessage->from = createDefaultUser(TEST_BOT_USER_ID_OFFSET);
        testRun("Send message expectation", location);

        EXPECT_CALL(*botApi, sendAnimation_impl(
                                 TEST_CHAT_ID,
                                 std::forward<FileIdMatcher>(fileIdMatcher),
                                 std::forward<CaptionMatcher>(textMatcher),
                                 std::forward<ReplyMatcher>(replyMatcher),
                                 std::forward<MarkMatcher>(markMatcher),
                                 TgBotApi::parseModeToStr<mode>()))
            .WillOnce(Return(sentMessage));
        return {sentMessage, botApi};
    }

   public:
    template <TgBotApi::ParseMode mode = TgBotApi::ParseMode::None,
              typename TextMatcher, typename MarkMatcher = decltype(IsNull())>
    SentMessage willSendMessage(TextMatcher&& textMatcher,
                                MarkMatcher&& markMatcher = IsNull(),
                                const st& location = st::current()) {
        return _willSendMessage<mode>(
            std::forward<TextMatcher>(textMatcher), IsNull(),
            std::forward<MarkMatcher>(markMatcher), location);
    }

    template <TgBotApi::ParseMode mode = TgBotApi::ParseMode::None,
              typename TextMatcher, typename MarkMatcher = decltype(IsNull())>
    SentMessage willSendReplyMessage(TextMatcher&& textMatcher,
                                     MarkMatcher&& markMatcher = IsNull(),
                                     const st& location = st::current()) {
        return _willSendMessage<mode>(
            std::forward<TextMatcher>(textMatcher), createMessageReplyMatcher(),
            std::forward<MarkMatcher>(markMatcher), location);
    }

    template <TgBotApi::ParseMode mode = TgBotApi::ParseMode::None,
              typename TextMatcher, typename MarkMatcher = decltype(IsNull())>
    SentMessage willSendReplyMessageTo(TextMatcher&& textMatcher,
                                       Message::Ptr replyToMessage,
                                       MarkMatcher&& markMatcher = IsNull(),
                                       const st& location = st::current()) {
        return _willSendMessage<mode>(
            std::forward<TextMatcher>(textMatcher),
            createMessageReplyMatcher(std::move(replyToMessage)),
            std::forward<MarkMatcher>(markMatcher), location);
    }

    template <TgBotApi::ParseMode mode = TgBotApi::ParseMode::None,
              typename FileIdMatcher, typename CaptionMatcher,
              typename MarkMatcher = decltype(IsNull())>
    SentMessage willSendFile(FileIdMatcher&& fileIdMatcher,
                             CaptionMatcher&& textMatcher,
                             MarkMatcher&& markMatcher = IsNull(),
                             const st& location = st::current()) {
        return _willSendFile<mode>(
            std::forward<FileIdMatcher>(fileIdMatcher),
            std::forward<CaptionMatcher>(textMatcher), IsNull(),
            std::forward<MarkMatcher>(markMatcher), location);
    }

    template <TgBotApi::ParseMode mode = TgBotApi::ParseMode::None,
              typename FileIdMatcher, typename CaptionMatcher,
              typename MarkMatcher = decltype(IsNull())>
    SentMessage willSendReplyFile(FileIdMatcher&& fileIdMatcher,
                                  CaptionMatcher&& textMatcher,
                                  MarkMatcher&& markMatcher = IsNull(),
                                  const st& location = st::current()) {
        return _willSendFile<mode>(std::forward<FileIdMatcher>(fileIdMatcher),
                                   std::forward<CaptionMatcher>(textMatcher),
                                   createMessageReplyMatcher(),
                                   std::forward<MarkMatcher>(markMatcher),
                                   location);
    }

    explicit CommandTestBase(std::string name) : name(std::move(name)) {}

   protected:
    std::string name;
    CommandModule::Ptr module;
    Message::Ptr defaultProvidedMessage;
    MockLocaleStrings strings;
};
