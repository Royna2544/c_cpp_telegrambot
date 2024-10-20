#include <absl/log/log.h>

#include <AbslLogInit.hpp>
#include <cstdlib>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <iostream>
#include <vector>
#include <ConfigManager.h>

#include "CommandLine.hpp"
#include "TryParseStr.hpp"

enum class Commands {
    Noop,
    Help,
    Dump,
    AddChat,
    SetOwnerId,
    WhiteBlackList,
    DeleteMedia = Noop,
    DeleteChat = Noop,
};

// Structure to hold command data for easier manipulation and handling
struct CommandData {
    // Array of command arguments after (exe, command)
    std::vector<std::string> args;
    // Instance of TgBotDatabaseImpl
    InstanceClassBase<TgBotDatabaseImpl>::pointer_type impl;
};

namespace {

template <Commands>
void executeCommand(const CommandData& data) {
    // Implement command logic here
}

template <>
void executeCommand<Commands::Noop>(const CommandData& /*data*/) {
    // No-op command for testing
    std::cout << "No-op command executed" << std::endl;
}

template <>
void executeCommand<Commands::Help>(const CommandData& data) {
    std::cout << std::endl << "Telegram Bot Database CLI" << std::endl;
    std::cout << "Usage: " << data.args[0] << " [command] [args...]"
              << std::endl;
    std::cout << "Available commands:" << std::endl;
    std::cout << "- dump: Dump the database contents to stdout" << std::endl;
    std::cout << "- delete_media: Delete media from the database" << std::endl;
    std::cout << "- add_chat: Add a chat info to the database" << std::endl;
    std::cout << "- delete_chat: Delete a chat info from the database"
              << std::endl;
    std::cout << "- set_owner_id: Set the owner ID for the database"
              << std::endl;
    std::cout << "- set_white_black_list: Add/remove the white/black list user "
                 "for the database"
              << std::endl;
}

template <>
void executeCommand<Commands::Dump>(const CommandData& data) {
    data.impl->dump(std::cout);
}

template <>
void executeCommand<Commands::AddChat>(const CommandData& data) {
    if (data.args.size() != 2) {
        LOG(ERROR) << "Need <chatid> <name> as arguments";
        return;
    }
    ChatId chatid{};
    if (!try_parse(data.args[0], &chatid)) {
        LOG(ERROR) << "Invalid chatid specified";
        return;
    }
    const std::string name = data.args[1];
    if (data.impl->addChatInfo(chatid, name)) {
        LOG(INFO) << "Added chat info for chatid=" << chatid
                  << " name=" << name;
    } else {
        LOG(ERROR) << "Failed to add chat info";
    }
}

template <>
void executeCommand<Commands::SetOwnerId>(const CommandData& data) {
    if (data.impl->getOwnerUserId()) {
        LOG(ERROR) << "Owner ID already set";
        return;
    }
    if (data.args.size() != 1) {
        LOG(ERROR) << "Need <owner_id> as argument";
        return;
    }
    UserId ownerId{};
    if (!try_parse(data.args[0], &ownerId)) {
        LOG(ERROR) << "Invalid owner_id specified";
        return;
    }
    data.impl->setOwnerUserId(ownerId);
    LOG(INFO) << "Owner ID set to " << ownerId;
}

template <>
void executeCommand<Commands::WhiteBlackList>(const CommandData& data) {
    if (data.args.size() < 3) {
        LOG(ERROR)
            << "Need <whitelist|blacklist> <add|remove> <user_id> as arguments";
        return;
    }
    std::string typeStr = data.args[0];
    if (typeStr != "whitelist" && typeStr != "blacklist") {
        LOG(ERROR) << "Invalid type specified. Use 'whitelist' or 'blacklist'";
        return;
    }
    DatabaseBase::ListType type{};
    if (typeStr == "whitelist") {
        type = DatabaseBase::ListType::WHITELIST;
    } else {
        type = DatabaseBase::ListType::BLACKLIST;
    }
    std::string action = data.args[1];
    if (action != "add" && action != "remove") {
        LOG(ERROR) << "Invalid action specified. Use 'add' or'remove'";
        return;
    }
    UserId userId{};
    if (!try_parse(data.args[2], &userId)) {
        LOG(ERROR) << "Invalid user_id specified";
        return;
    }
    if (action == "add") {
        if (data.impl->addUserToList(type, userId) ==
            DatabaseBase::ListResult::OK) {
            LOG(INFO) << "Added user_id=" << userId << " to list";
        } else {
            LOG(ERROR) << "Failed to add user_id=" << userId << " to list";
        }
    } else if (action == "remove") {
        if (data.impl->removeUserFromList(type, userId) ==
            DatabaseBase::ListResult::OK) {
            LOG(INFO) << "Removed user_id=" << userId << " from list";
        } else {
            LOG(ERROR) << "Failed to remove user_id=" << userId << " from list";
        }
    }
}

bool loadDB_TO_BE_FIXED_TODO() {
    auto dbimpl = TgBotDatabaseImpl::getInstance();
    using namespace ConfigManager;
    const auto dbConf = getVariable(Configs::DATABASE_BACKEND);
    std::error_code ec;
    bool loaded = false;

    if (!dbConf) {
        LOG(ERROR) << "No database backend specified in config";
        return false;
    }

    const std::string& config = dbConf.value();
    const auto speratorIdx = config.find(':');

    if (speratorIdx == std::string::npos) {
        LOG(ERROR) << "Invalid database configuration";
        return false;
    }

    // Expected format: <backend>:filename relative to git root (Could be
    // absolute)
    const auto backendStr = config.substr(0, speratorIdx);
    const auto filenameStr = config.substr(speratorIdx + 1);

    TgBotDatabaseImpl::Providers provider;
    if (!provider.chooseProvider(backendStr)) {
        LOG(ERROR) << "Failed to choose provider";
        return false;
    }
    dbimpl->setImpl(std::move(provider));
    loaded = dbimpl->load(filenameStr);
    if (!loaded) {
        LOG(ERROR) << "Failed to load database";
    } else {
        DLOG(INFO) << "Database loaded";
    }
    return loaded;
}
}  // namespace

int main(int argc, char** argv) {
    TgBot_AbslLogInit();
    auto inst = CommandLine::initInstance(argc, argv);

    std::vector<std::string> args = inst->getArguments();
    CommandData data = {args, nullptr};

    if (args.size() < 2) {
        executeCommand<Commands::Help>(data);
        return EXIT_SUCCESS;
    }

    auto dbImpl = TgBotDatabaseImpl::getInstance();
    loadDB_TO_BE_FIXED_TODO();
    if (!dbImpl->isLoaded()) {
        LOG(ERROR) << "Failed to load database";
        return EXIT_FAILURE;
    }
    const std::string command = args[1];
    // Erase exe, command name
    args.erase(args.begin(), args.begin() + 2);
    data = {args, dbImpl};

    DLOG(INFO) << "Executing command: " << command;
    if (command == "dump") {
        executeCommand<Commands::Dump>(data);
    } else if (command == "delete_media") {
        executeCommand<Commands::DeleteMedia>(data);
    } else if (command == "add_chat") {
        executeCommand<Commands::AddChat>(data);
    } else if (command == "delete_chat") {
        executeCommand<Commands::DeleteChat>(data);
    } else if (command == "set_owner_id") {
        executeCommand<Commands::SetOwnerId>(data);
    } else if (command == "set_white_black_list") {
        executeCommand<Commands::WhiteBlackList>(data);
    } else {
        LOG(ERROR) << "Unknown command: " << command;
    }
    dbImpl->unloadDatabase();
}
