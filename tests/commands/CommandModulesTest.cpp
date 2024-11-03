#include "CommandModulesTest.hpp"

#include <fmt/format.h>

#include <database/bot/TgBotDatabaseImpl.hpp>
#include <libfs.hpp>
#include <memory>

#include "ConfigManager.hpp"
#include "api/CommandModule.hpp"

void CommandModulesTest::SetUp() {
    TgBotDatabaseImpl::Providers provider;

    modulePath = provideInject.get<ConfigManager *>()->exe().parent_path();
    database = new MockDatabase();
    provider.registerProvider("testing",
                              std::unique_ptr<MockDatabase>(database));
    ASSERT_TRUE(provider.chooseProvider("testing"));
    ASSERT_TRUE(databaseImpl.setImpl(std::move(provider)));
    EXPECT_CALL(*database, load(_)).WillOnce(Return(true));
    ASSERT_TRUE(databaseImpl.load({}));
    ON_CALL(*database, unloadDatabase).WillByDefault(Return(true));
}

void CommandModulesTest::TearDown() {
    Mock::VerifyAndClearExpectations(botApi.get());
    Mock::VerifyAndClearExpectations(database);
}

CommandModule::Ptr CommandModulesTest::loadModule(
    const std::string& name) const {
    std::filesystem::path moduleFileName =
        fmt::format("{}{}", CommandModule::prefix, name);
    const auto moduleFilePath = modulePath / moduleFileName / FS::SharedLib;

    LOG(INFO) << "Loading module " << std::quoted(name) << " for testing...";
    auto module = std::make_unique<CommandModule>(moduleFilePath);

    auto ret = module->load();
    EXPECT_TRUE(ret);

    if (ret) {
        LOG(INFO) << "Module " << name << " loaded successfully.";
    } else {
        LOG(WARNING) << "Failed to load module " << name;
        return nullptr;
    }
    return module;
}

void CommandModulesTest::unloadModule(CommandModule::Ptr module) {
    module->unload();
}

Message::Ptr CommandModulesTest::createDefaultMessage() {
    static MessageId messageId = TEST_MESSAGE_ID;
    auto message = std::make_shared<Message>();
    message->chat = std::make_shared<Chat>();
    message->chat->id = TEST_CHAT_ID;
    message->from = createDefaultUser();
    message->messageId = messageId++;
    message->entities.emplace_back(std::make_shared<TgBot::MessageEntity>());
    message->entities[0]->type = TgBot::MessageEntity::Type::BotCommand;
    message->entities[0]->offset = 0;
    return message;
}

User::Ptr CommandModulesTest::createDefaultUser(long id_offset) {
    auto user = std::make_shared<User>();
    // id_offset is used to generate unique user IDs for testing purposes
    user->id = TEST_USER_ID + id_offset;
    user->username = TEST_USERNAME + std::to_string(id_offset);
    user->firstName = TEST_NICKNAME + std::to_string(id_offset ^ 0b0101001);
    return user;
}

bool CommandModulesTest::isReplyToThisMsg(const ReplyParametersExt::Ptr& rhs,
                                          const Message::Ptr& message) {
    if (!rhs) {
        LOG(INFO) << "ReplyParameters is nullptr";
        return false;
    }
    ChatId lChatId = rhs->chatId;
    ChatId rChatId = message->chat->id;
    MessageId lMessageId = rhs->messageId;
    MessageId rMessageId = message->messageId;
    if (lChatId != rChatId || lMessageId != rMessageId) {
        LOG(INFO) << "ReplyToThisMsg false: " << lChatId << " " << lMessageId
                  << " vs " << rChatId << " " << rMessageId;
        return false;
    }
    return true;
}

std::string CommandModulesTest::current_path() {
    std::error_code ec;
    auto ret = std::filesystem::current_path(ec).generic_string();

    // Check if we have drive letter (e.g. C:)
    if (ret.size() > 2 && ret[1] == ':') {
        char driveLetter = ret[0];
        ret[0] = '/';
        ret[1] = std::tolower(driveLetter);
    }
    if (ec) {
        LOG(ERROR) << "Couldn't get current path";
        return {};
    }
    return ret;
}
