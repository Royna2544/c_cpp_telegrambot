#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <TgBotWrapper.hpp>
#include <database/DatabaseBase.hpp>
#include <source_location>
#include <utility>

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

class MockDatabase : public DatabaseBase {
   public:
    MOCK_METHOD(DatabaseBase::ListResult, addUserToList,
                (DatabaseBase::ListType type, UserId user), (const, override));

    MOCK_METHOD(DatabaseBase::ListResult, removeUserFromList,
                (DatabaseBase::ListType type, UserId user), (const, override));

    MOCK_METHOD(DatabaseBase::ListResult, checkUserInList,
                (DatabaseBase::ListType type, UserId user), (const, override));

    MOCK_METHOD(std::optional<UserId>, getOwnerUserId, (), (const, override));

    MOCK_METHOD(std::optional<DatabaseBase::MediaInfo>, queryMediaInfo,
                (std::string str), (const, override));

    MOCK_METHOD(bool, addMediaInfo, (const DatabaseBase::MediaInfo& info),
                (const, override));

    MOCK_METHOD(std::ostream&, dump, (std::ostream & ofs), (const, override));

    MOCK_METHOD(void, setOwnerUserId, (UserId userid), (const, override));

    MOCK_METHOD(bool, addChatInfo,
                (const ChatId chatid, const std::string& name),
                (const, override));

    MOCK_METHOD(std::optional<ChatId>, getChatId, (const std::string& name),
                (const, override));

    MOCK_METHOD(bool, loadDatabaseFromFile, (std::filesystem::path filepath),
                (override));

    MOCK_METHOD(bool, unloadDatabase, (), (override));
};

class MockTgBotApi : public TgBotApi {
   public:
    MOCK_METHOD(Message::Ptr, sendMessage_impl,
                (ChatId chatId, const std::string& text,
                 ReplyParametersExt::Ptr replyParameters,
                 GenericReply::Ptr replyMarkup, const std::string& parseMode),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendAnimation_impl,
                (ChatId chatId, FileOrString animation,
                 const std::string& caption,
                 ReplyParametersExt::Ptr replyParameters,
                 GenericReply::Ptr replyMarkup, const std::string& parseMode),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendSticker_impl,
                (ChatId chatId, FileOrString sticker,
                 ReplyParametersExt::Ptr replyParameters),
                (const, override));

    MOCK_METHOD(Message::Ptr, editMessage_impl,
                (const Message::Ptr& message, const std::string& newText,
                 const TgBot::InlineKeyboardMarkup::Ptr& markup),
                (const, override));

    MOCK_METHOD(Message::Ptr, editMessageMarkup_impl,
                (const StringOrMessage& message,
                 const GenericReply::Ptr& replyMarkup),
                (const, override));

    MOCK_METHOD(void, deleteMessage_impl, (const Message::Ptr& message),
                (const, override));

    MOCK_METHOD(void, deleteMessages_impl,
                (ChatId chatId, const std::vector<MessageId>& messageIds),
                (const, override));

    MOCK_METHOD(void, restrictChatMember_impl,
                (ChatId chatId, UserId userId, ChatPermissions::Ptr permissions,
                 std::uint32_t untilDate),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendDocument_impl,
                (ChatId chatId, FileOrString document,
                 const std::string& caption,
                 ReplyParametersExt::Ptr replyParameters,
                 GenericReply::Ptr replyMarkup, const std::string& parseMode),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendPhoto_impl,
                (ChatId chatId, FileOrString photo, const std::string& caption,
                 ReplyParametersExt::Ptr replyParameters,
                 GenericReply::Ptr replyMarkup, const std::string& parseMode),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendVideo_impl,
                (ChatId chatId, FileOrString photo, const std::string& caption,
                 ReplyParametersExt::Ptr replyParameters,
                 GenericReply::Ptr replyMarkup, const std::string& parseMode),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendDice_impl, (const ChatId chatId),
                (const, override));

    MOCK_METHOD(StickerSet::Ptr, getStickerSet_impl,
                (const std::string& setName), (const, override));

    MOCK_METHOD(bool, createNewStickerSet_impl,
                (std::int64_t userId, const std::string& name,
                 const std::string& title,
                 const std::vector<InputSticker::Ptr>& stickers,
                 Sticker::Type stickerType),
                (const, override));

    MOCK_METHOD(File::Ptr, uploadStickerFile_impl,
                (std::int64_t userId, InputFile::Ptr sticker,
                 const std::string& stickerFormat),
                (const, override));

    MOCK_METHOD(bool, downloadFile_impl,
                (const std::filesystem::path& destfilename,
                 const std::string& fileid),
                (const, override));

    MOCK_METHOD(User::Ptr, getBotUser_impl, (), (const, override));

    MOCK_METHOD(MessageId, copyMessage_impl,
                (ChatId chat, MessageId message, ReplyParametersExt::Ptr reply),
                (const, override));
    MOCK_METHOD(bool, answerCallbackQuery_impl,
                (const std::string& callbackQueryId, const std::string& text,
                 bool showAlert),
                (const override));

    MOCK_METHOD(bool, pinMessage_impl, (MessagePtr message), (const override));
    MOCK_METHOD(bool, unpinMessage_impl, (MessagePtr message),
                (const override));
    MOCK_METHOD(bool, banChatMember_impl,
                (const Chat::Ptr& chat, const User::Ptr& user),
                (const override));
    MOCK_METHOD(bool, unbanChatMember_impl,
                (const Chat::Ptr& chat, const User::Ptr& user),
                (const override));
    MOCK_METHOD(User::Ptr, getChatMember_impl, (const ChatId, const UserId),
                (const override));

    // Non-TgBotApi methods
    MOCK_METHOD(bool, reloadCommand, (const std::string& cmd), (override));
    MOCK_METHOD(bool, unloadCommand, (const std::string& cmd), (override));
    MOCK_METHOD(void, registerCallback,
                (const onanymsg_callback_type& callback, size_t token),
                (override));
    MOCK_METHOD(bool, unregisterCallback, (size_t token), (override));
};

class CommandModulesTest : public ::testing::Test {
   protected:
    struct ModuleHandle {
        void* handle;
        CommandModule module;

        // Shorthand
        void execute(ApiPtr api, MessagePtr msg) const { module.fn(api, msg); }
    };

    /**
     * @brief Set up the test suite before running any tests.
     * This function is called once before running all tests in the test
     * suite. It is responsible for any necessary setup tasks.
     */
    static void SetUpTestSuite();

    /**
     * @brief Tear down the test suite after running all tests.
     * This function is called once after running all tests in the test suite.
     * It is responsible for any necessary cleanup tasks.
     */
    static void TearDownTestSuite();

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
     * @return An optional containing the loaded module handle if successful,
     *         or an empty optional if the module could not be loaded.
     */
    std::optional<ModuleHandle> loadModule(const std::string& name) const;

    /**
     * @brief Unload a command module.
     *
     * @param module The module handle to unload.
     */
    static void unloadModule(ModuleHandle&& module);

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
    static User::Ptr createDefaultUser(off_t id_offset = 0);

    /**
     * @brief Check if a reply parameters object refers to a specific message.
     *
     * @param rhs The reply parameters object to check.
     * @param message The message to compare against.
     * @return True if the reply parameters refer to the specified message,
     *         false otherwise.
     */
    static bool isReplyToThisMsg(ReplyParametersExt::Ptr rhs,
                                 MessagePtr message);

    /**
     * @brief Get the current working directory path.
     *
     * @return The current working directory path as a string.
     */
    static std::string current_path();

    // Testing data
    static constexpr ChatId TEST_CHAT_ID = 123123;
    static constexpr UserId TEST_USER_ID = 456456;
    static constexpr MessageId TEST_MESSAGE_ID = 789789;
    static constexpr const char* TEST_USERNAME = "testuser";
    static constexpr const char* TEST_NICKNAME = "testnickname";
    static constexpr const char* ALIVE_FILE_ID = "alive";
    static constexpr const char* TEST_MEDIA_ID = "TOO_ID";
    static constexpr const char* TEST_MEDIA_UNIQUEID = "TOO_UNIQUE";
    static constexpr off_t TEST_BOT_USER_ID_OFFSET = 1000;
    const MediaIds TEST_MEDIAIDS{TEST_MEDIA_ID, TEST_MEDIA_UNIQUEID};
    const DatabaseBase::MediaInfo TEST_MEDIAINFO{TEST_MEDIA_ID,
                                                 TEST_MEDIA_UNIQUEID};

    std::shared_ptr<MockTgBotApi> botApi = std::make_shared<MockTgBotApi>();
    MockDatabase database;
    std::filesystem::path modulePath;
};

class CommandTestBase : public CommandModulesTest {
   protected:
    void SetUp() override {
        CommandModulesTest::SetUp();
        const auto mod = loadModule(name);
        ASSERT_TRUE(mod);
        module = mod.value();
        defaultProvidedMessage = createDefaultMessage();
    }
    void TearDown() override {
        unloadModule(std::move(module));
        CommandModulesTest::TearDown();
        defaultProvidedMessage.reset();
    }

    constexpr static bool kTestDebug = true;
    using st = std::source_location;
    static void testRun(const std::string& message, const st& location) {
        if (!kTestDebug) {
            return;
        }
        std::cout << message << " [file: " << location.file_name()
                  << ", line: " << location.line() << "]" << std::endl;
    }

   public:
    void setCommandExtArgs(const std::string& command) {
        setCommandExtArgs();
        defaultProvidedMessage->text += " " + command;
    }
    void setCommandExtArgs() { defaultProvidedMessage->text = "/" + name; }
    void execute() { module.execute(botApi, defaultProvidedMessage); }

    struct SentMessage {
        Message::Ptr message;
        std::shared_ptr<MockTgBotApi> botApi;

        template <typename T>
        const SentMessage& willEdit(T&& matcher,
                                    const st& location = st::current()) const {
            testRun("Edit message expectation", location);
            EXPECT_CALL(*botApi, editMessage_impl(_, matcher, IsNull()))
                .WillOnce(DoAll(
                    WithArg<0>([=, addr = message.get()](auto&& message_in) {
                        EXPECT_EQ(message_in.get(), addr);
                    }),
                    ReturnArg<0>()));
            return *this;
        }
    };

    auto createMessageReplyMatcher(MessagePtr message = nullptr) {
        return Truly([=, this](ReplyParametersExt::Ptr m) {
            return isReplyToThisMsg(std::move(m),
                                    message ?: defaultProvidedMessage);
        });
    }

   private:
    // Template function for sending a message with expectations
    template <TgBotWrapper::ParseMode mode = TgBotWrapper::ParseMode::None,
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
                                     TgBotWrapper::parseModeToStr<mode>()))
            .WillOnce(Return(sentMessage));
        return {sentMessage, botApi};
    }
    template <TgBotWrapper::ParseMode mode = TgBotWrapper::ParseMode::None,
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
                                 TgBotWrapper::parseModeToStr<mode>()))
            .WillOnce(Return(sentMessage));
        return {sentMessage, botApi};
    }

   public:
    template <TgBotWrapper::ParseMode mode = TgBotWrapper::ParseMode::None,
              typename TextMatcher, typename MarkMatcher = decltype(IsNull())>
    SentMessage willSendMessage(TextMatcher&& textMatcher,
                                MarkMatcher&& markMatcher = IsNull(),
                                const st& location = st::current()) {
        return _willSendMessage<mode>(
            std::forward<TextMatcher>(textMatcher), IsNull(),
            std::forward<MarkMatcher>(markMatcher), location);
    }

    template <TgBotWrapper::ParseMode mode = TgBotWrapper::ParseMode::None,
              typename TextMatcher, typename MarkMatcher = decltype(IsNull())>
    SentMessage willSendReplyMessage(TextMatcher&& textMatcher,
                                     MarkMatcher&& markMatcher = IsNull(),
                                     const st& location = st::current()) {
        return _willSendMessage<mode>(
            std::forward<TextMatcher>(textMatcher), createMessageReplyMatcher(),
            std::forward<MarkMatcher>(markMatcher), location);
    }

    template <TgBotWrapper::ParseMode mode = TgBotWrapper::ParseMode::None,
              typename TextMatcher, typename MarkMatcher = decltype(IsNull())>
    SentMessage willSendReplyMessageTo(TextMatcher&& textMatcher,
                                       MessagePtr replyToMessage,
                                       MarkMatcher&& markMatcher = IsNull(),
                                       const st& location = st::current()) {
        return _willSendMessage<mode>(std::forward<TextMatcher>(textMatcher),
                                      createMessageReplyMatcher(replyToMessage),
                                      std::forward<MarkMatcher>(markMatcher),
                                      location);
    }

    template <TgBotWrapper::ParseMode mode = TgBotWrapper::ParseMode::None,
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

    template <TgBotWrapper::ParseMode mode = TgBotWrapper::ParseMode::None,
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
    ModuleHandle module;
    Message::Ptr defaultProvidedMessage;
};
