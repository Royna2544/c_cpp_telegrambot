#include <absl/log/log.h>

#include <AbslLogInit.hpp>
#include <cstdlib>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <iostream>
#include <memory>
#include <vector>

#include "CommandLine.hpp"
#include "TryParseStr.hpp"

enum class Commands {
    Noop,
    Help,
    Dump,
    AddChat,
    SetOwnerId,
    DeleteMedia = Noop,
    DeleteChat = Noop,
};

// Structure to hold command data for easier manipulation and handling
struct CommandData {
    std::vector<std::string>
        args;  // Array of command arguments after (exe, command)
    std::shared_ptr<TgBotDatabaseImpl> impl;  // Instance of TgBotDatabaseImpl
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
    dbImpl->initWrapper();
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
    } else {
        LOG(ERROR) << "Unknown command: " << command;
    }
    dbImpl->unloadDatabase();
}
