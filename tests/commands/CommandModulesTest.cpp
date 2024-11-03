#include "CommandModulesTest.hpp"

#include <fmt/format.h>

#include <database/bot/TgBotDatabaseImpl.hpp>
#include <libfs.hpp>
#include <memory>

#include "CommandLine.hpp"
#include "ConfigManager.hpp"
#include "Random.hpp"
#include "api/CommandModule.hpp"
#include "api/Providers.hpp"
#include "api/TgBotApi.hpp"
#include "fruit/fruit.h"
#include "fruit/fruit_forward_decls.h"
#include "tests/ClassProviders.hpp"
#include "trivial_helpers/fruit_inject.hpp"

fruit::Component<CommandLine> getCommandLine() {
    return fruit::createComponent().registerProvider([] {
        static auto strings = testing::internal::GetArgvs();
        static std::vector<char*> c_strings;

        c_strings.reserve(strings.size() + 1);
        for (auto& s : strings) {
            c_strings.emplace_back(s.data());
        }
        c_strings.emplace_back(nullptr);

        return CommandLine(strings.size(), c_strings.data());
    });
}

fruit::Component<MockTgBotApi, Providers, MockDatabase, MockResource,
                 CommandLine, MockRandom>
CommandModulesTest::getProviders() {
    return fruit::createComponent()
        .bind<ResourceProvider, MockResource>()
        .bind<DatabaseBase, MockDatabase>()
        .bind<TgBotApi, MockTgBotApi>()
        .bind<RandomBase, MockRandom>()
        .install(getCommandLine);
}

void CommandModulesTest::SetUp() {
    modulePath = provideInject.get<CommandLine*>()->exe().parent_path();
    database = provideInject.get<MockDatabase*>();
    random = provideInject.get<MockRandom*>();
    resource = provideInject.get<MockResource*>();
    botApi = provideInject.get<MockTgBotApi*>();
}

void CommandModulesTest::TearDown() {
    Mock::VerifyAndClearExpectations(botApi);
    Mock::VerifyAndClearExpectations(database);
    Mock::VerifyAndClearExpectations(random);
    Mock::VerifyAndClearExpectations(resource);
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
