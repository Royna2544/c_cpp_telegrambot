#include <dlfcn.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <StringResManager.hpp>
#include <TgBotWrapper.hpp>
#include <filesystem>
#include <libos/libfs.hpp>
#include <memory>
#include <optional>

#include "Types.h"
#include "database/bot/TgBotDatabaseImpl.hpp"
#include "gmock/gmock.h"

using testing::_;
using testing::IsNull;
using testing::Return;
using testing::StartsWith;
using testing::Truly;

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
                 ReplyParameters::Ptr replyParameters,
                 GenericReply::Ptr replyMarkup, const std::string& parseMode),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendAnimation_impl,
                (ChatId chatId, FileOrString animation,
                 const std::string& caption,
                 ReplyParameters::Ptr replyParameters,
                 GenericReply::Ptr replyMarkup, const std::string& parseMode),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendSticker_impl,
                (ChatId chatId, FileOrString sticker,
                 ReplyParameters::Ptr replyParameters),
                (const, override));

    MOCK_METHOD(Message::Ptr, editMessage_impl,
                (const Message::Ptr& message, const std::string& newText),
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
                 ReplyParameters::Ptr replyParameters,
                 GenericReply::Ptr replyMarkup, const std::string& parseMode),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendPhoto_impl,
                (ChatId chatId, FileOrString photo, const std::string& caption,
                 ReplyParameters::Ptr replyParameters,
                 GenericReply::Ptr replyMarkup, const std::string& parseMode),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendVideo_impl,
                (ChatId chatId, FileOrString photo, const std::string& caption,
                 ReplyParameters::Ptr replyParameters,
                 GenericReply::Ptr replyMarkup, const std::string& parseMode),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendDice_impl, (const ChatId chatId),
                (const, override));

    MOCK_METHOD(StickerSet::Ptr, getStickerSet_impl,
                (const std::string& setName), (const, override));

    MOCK_METHOD(bool, downloadFile_impl,
                (const std::filesystem::path& destfilename,
                 const std::string& fileid),
                (const, override));

    MOCK_METHOD(User::Ptr, getBotUser_impl, (), (const, override));
};

class CommandModulesTest : public ::testing::Test {
   protected:
    static void SetUpTestSuite() {}
    static void TearDownTestSuite() { TgBotDatabaseImpl::destroyInstance(); }

    void SetUp() override {
        modulePath = FS::getPathForType(FS::PathType::MODULES_INSTALLED);
    }

    void TearDown() override {}

    struct ModuleHandle {
        void* handle;
        CommandModule module;

        // Shorthand
        void execute(ApiPtr api, MessagePtr msg) const {
            module.fn(api, msg);
        }
    };

    std::optional<ModuleHandle> loadModule(const std::string& name) const {
        std::filesystem::path moduleFileName = "libcmd_" + name;
        const auto moduleFilePath =
            modulePath / FS::appendDylibExtension(moduleFileName);
        bool (*sym)(const char*, CommandModule&) = nullptr;

        LOG(INFO) << "Loading module " << std::quoted(name)
                  << " for testing...";
        void* handle = dlopen(moduleFilePath.string().c_str(), RTLD_NOW);
        if (handle == nullptr) {
            LOG(ERROR) << "Error loading module: " << dlerror();
            return std::nullopt;
        }
        sym =
            reinterpret_cast<decltype(sym)>(dlsym(handle, DYN_COMMAND_SYM_STR));
        if (sym == nullptr) {
            LOG(ERROR) << "Error getting symbol: " << dlerror();
            dlclose(handle);
            return std::nullopt;
        }
        CommandModule cmdmodule;
        try {
            if (!sym(name.c_str(), cmdmodule)) {
                LOG(ERROR) << "Error initializing module";
                cmdmodule.fn = nullptr;
                unloadModule({handle, cmdmodule});
                return std::nullopt;
            }
        } catch (const std::exception& e) {
            LOG(ERROR) << "Exception thrown while executing module function: "
                       << e.what();
            cmdmodule.fn = nullptr;
            unloadModule({handle, cmdmodule});
            return std::nullopt;
        }
        LOG(INFO) << "Module " << name << " loaded successfully.";
        return {{handle, cmdmodule}};
    }

    static void unloadModule(ModuleHandle&& module) {
        // Clear function pointer to prevent double free
        module.module.fn = nullptr;
        dlclose(module.handle);
    }

    static Message::Ptr createDefaultMessage() {
        auto message = std::make_shared<Message>();
        message->chat = std::make_shared<Chat>();
        message->chat->id = TEST_CHAT_ID;
        message->from = std::make_shared<User>();
        message->from->id = TEST_USER_ID;
        message->from->username = TEST_USERNAME;
        message->from->firstName = TEST_NICKNAME;
        message->messageId = TEST_MESSAGE_ID;
        return message;
    }
    static bool isSameAsExpected(ReplyParameters::Ptr rhs) {
        const bool cond =
            rhs->chatId == TEST_CHAT_ID && rhs->messageId == TEST_MESSAGE_ID;
        if (!cond) {
            LOG(INFO) << "ChatID: " << rhs->chatId << " != " << TEST_CHAT_ID;
            LOG(INFO) << "MessageID: " << rhs->messageId
                      << " != " << TEST_MESSAGE_ID;
        }
        return cond;
    };

    // Testing data
    static constexpr ChatId TEST_CHAT_ID = 123123;
    static constexpr UserId TEST_USER_ID = 456456;
    static constexpr MessageId TEST_MESSAGE_ID = 789789;
    static constexpr const char* TEST_USERNAME = "testuser";
    static constexpr const char* TEST_NICKNAME = "testnickname";
    static constexpr const char* ALIVE_FILE_ID = "alive";
    static constexpr const char* TEST_MEDIA_ID = "TOO_ID";
    static constexpr const char* TEST_MEDIA_UNIQUEID = "TOO_UNIQUE";
    const MediaIds TEST_MEDIAIDS{TEST_MEDIA_ID, TEST_MEDIA_UNIQUEID};
    const DatabaseBase::MediaInfo TEST_MEDIAINFO{TEST_MEDIA_ID,
                                                 TEST_MEDIA_UNIQUEID};

    std::shared_ptr<MockTgBotApi> botApi = std::make_shared<MockTgBotApi>();
    MockDatabase database;
    std::filesystem::path modulePath;
};

TEST_F(CommandModulesTest, loadDatabase) {
    auto dbinst = TgBotDatabaseImpl::getInstance();

    EXPECT_CALL(database, loadDatabaseFromFile(_)).WillOnce(Return(true));
    ASSERT_TRUE(dbinst->setImpl(&database));
    ASSERT_TRUE(dbinst->loadDatabaseFromFile({}));
}

TEST_F(CommandModulesTest, TestCommandAlive) {
    auto module = loadModule("alive");
    ASSERT_TRUE(module.has_value());

    auto message = createDefaultMessage();
    message->text = "/alive";
    auto botUser = std::make_shared<User>();

    // Would call two times for username, nickname
    EXPECT_CALL(*botApi, getBotUser_impl())
        .Times(2)
        .WillRepeatedly(Return(botUser));

    // First, if alive medianame existed
    EXPECT_CALL(database, queryMediaInfo(ALIVE_FILE_ID))
        .WillOnce(Return(TEST_MEDIAINFO));

    // Expected to pass the fileid and parsemode as HTML
    EXPECT_CALL(*botApi,
                sendAnimation_impl(TEST_CHAT_ID, {TEST_MEDIA_ID}, _,
                                   Truly(isSameAsExpected), IsNull(), "HTML"))
        .WillOnce(Return(nullptr));
    module->execute(botApi, message);

    // Second, if alive medianame not existed
    EXPECT_CALL(database, queryMediaInfo(ALIVE_FILE_ID))
        .WillOnce(Return(std::nullopt));
    EXPECT_CALL(*botApi,
                sendMessage_impl(TEST_CHAT_ID, _, Truly(isSameAsExpected),
                                 IsNull(), "HTML"))
        .WillOnce(Return(nullptr));
    module->execute(botApi, message);

    // Done, unload the module
    unloadModule(std::move(module.value()));
}

// ChatId chatId, const std::string& text,
// ReplyParameters::Ptr replyParameters,
// GenericReply::Ptr replyMarkup, const std::string& parseMode
TEST_F(CommandModulesTest, TestCommandBash) {
    auto module = loadModule("bash");
    ASSERT_TRUE(module.has_value());

    auto message = createDefaultMessage();
    message->text = "/bash pwd";

    // First, "Working on it..."
    EXPECT_CALL(*botApi, sendMessage_impl(TEST_CHAT_ID, GETSTR(WORKING), _,
                                         IsNull(), ""));

    // Second, Command result of pwd command
    EXPECT_CALL(
        *botApi,
        sendMessage_impl(TEST_CHAT_ID,
                         StartsWith(std::filesystem::current_path().string()),
                         _, IsNull(), ""));

    module->execute(botApi, message);

    // Done, unload the module
    unloadModule(std::move(module.value()));
}

// ChatId chatId, const std::string& text,
// ReplyParameters::Ptr replyParameters,
// GenericReply::Ptr replyMarkup, const std::string& parseMode
TEST_F(CommandModulesTest, TestCommandCmd) {
    auto module = loadModule("cmd");
    ASSERT_TRUE(module.has_value());

    auto message = createDefaultMessage();
    message->text = "/cmd pwd";

    // First, "Working on it..."
    EXPECT_CALL(*botApi, sendMessage_impl(TEST_CHAT_ID, GETSTR(WORKING), _,
                                         IsNull(), ""));

    // Second, Command result of pwd command
    EXPECT_CALL(
        *botApi,
        sendMessage_impl(TEST_CHAT_ID,
                         StartsWith(std::filesystem::current_path().string()),
                         _, IsNull(), ""));

    module->execute(botApi, message);

    // Done, unload the module
    unloadModule(std::move(module.value()));
}