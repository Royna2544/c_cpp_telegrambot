#include "CommandModulesTest.hpp"

#include <command_modules/support/popen_wdt/popen_wdt.h>
#include <dlfcn.h>

#include <database/bot/TgBotDatabaseImpl.hpp>
#include <libos/libfs.hpp>

void CommandModulesTest::SetUpTestSuite() {}

void CommandModulesTest::TearDownTestSuite() {
    TgBotDatabaseImpl::destroyInstance();
}

void CommandModulesTest::SetUp() {
    modulePath = FS::getPathForType(FS::PathType::MODULES_INSTALLED);
    auto dbinst = TgBotDatabaseImpl::getInstance();

    EXPECT_CALL(database, loadDatabaseFromFile(_)).WillOnce(Return(true));
    ASSERT_TRUE(dbinst->setImpl(&database));
    ASSERT_TRUE(dbinst->loadDatabaseFromFile(
        std::filesystem::current_path().root_path()));
}

void CommandModulesTest::TearDown() {
    Mock::VerifyAndClearExpectations(botApi.get());
    TgBotDatabaseImpl::destroyInstance();
}

std::optional<CommandModulesTest::ModuleHandle> CommandModulesTest::loadModule(
    const std::string& name) const {
    std::filesystem::path moduleFileName = "libcmd_" + name;
    const auto moduleFilePath =
        modulePath / FS::appendDylibExtension(moduleFileName);
    bool (*sym)(const char*, CommandModule&) = nullptr;

    LOG(INFO) << "Loading module " << std::quoted(name) << " for testing...";
    void* handle = dlopen(moduleFilePath.string().c_str(), RTLD_NOW);
    if (handle == nullptr) {
        LOG(ERROR) << "Error loading module: " << dlerror();
        return std::nullopt;
    }
    sym = reinterpret_cast<decltype(sym)>(dlsym(handle, DYN_COMMAND_SYM_STR));
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

void CommandModulesTest::unloadModule(ModuleHandle&& module) {
    // Clear function pointer to prevent double free
    module.module.fn = nullptr;
    dlclose(module.handle);
}

Message::Ptr CommandModulesTest::createDefaultMessage() {
    static MessageId messageId = TEST_MESSAGE_ID;
    auto message = std::make_shared<Message>();
    message->chat = std::make_shared<Chat>();
    message->chat->id = TEST_CHAT_ID;
    message->from = createDefaultUser();
    message->messageId = messageId++;
    return message;
}

User::Ptr CommandModulesTest::createDefaultUser(off_t id_offset) {
    auto user = std::make_shared<User>();
    // id_offset is used to generate unique user IDs for testing purposes
    user->id = TEST_USER_ID + id_offset;
    user->username = TEST_USERNAME + std::to_string(id_offset);
    user->firstName = TEST_NICKNAME + std::to_string(id_offset ^ 0b0101001);
    return user;
}

bool CommandModulesTest::isReplyToThisMsg(ReplyParameters::Ptr rhs,
                                          MessagePtr message) {
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
