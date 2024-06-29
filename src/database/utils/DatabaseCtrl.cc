#include <absl/log/log.h>

#include <AbslLogInit.hpp>
#include <cstdlib>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <iostream>
#include <memory>

#include "TryParseStr.hpp"

enum class Commands { Help, Dump, DeleteMedia, AddChat, DeleteChat };

// Structure to hold command data for easier manipulation and handling
struct CommandData {
    int argc;           // Number of arguments after (exe, command)
    const char** argv;  // Array of command arguments after (exe, command)
    std::shared_ptr<TgBotDatabaseImpl> impl;  // Instance of TgBotDatabaseImpl
};

namespace {
template <Commands>
void executeCommand(const CommandData& data) {
    // Implement command logic here
}

template <>
void executeCommand<Commands::Help>(const CommandData& data) {
    std::cout << std::endl << "Telegram Bot Database CLI" << std::endl;
    std::cout << "Usage: " << data.argv[0] << " [command] [args...]"
              << std::endl;
    std::cout << "Available commands:" << std::endl;
    std::cout << "- dump: Dump the database contents to stdout" << std::endl;
    std::cout << "- delete_media: Delete media from the database" << std::endl;
    std::cout << "- add_chat: Add a chat info to the database" << std::endl;
    std::cout << "- delete_chat: Delete a chat info from the database"
              << std::endl;
}

template <>
void executeCommand<Commands::Dump>(const CommandData& data) {
    data.impl->dump(std::cout);
}

template <>
void executeCommand<Commands::DeleteMedia>(const CommandData& data) {
    // Not implemented as it will need DatabaseBase::deleteMedia() function
    // implementation
}

template <>
void executeCommand<Commands::AddChat>(const CommandData& data) {
    if (data.argc != 2) {
        LOG(ERROR) << "Need <chatid> <name> as arguments";
        return;
    }
    ChatId chatid{};
    if (!try_parse(data.argv[0], &chatid)) {
        LOG(ERROR) << "Invalid chatid specified";
        return;
    }
    const std::string name = data.argv[1];
    if (data.impl->addChatInfo(chatid, name)) {
        LOG(INFO) << "Added chat info for chatid=" << chatid
                  << " name=" << name;
    } else {
        LOG(ERROR) << "Failed to add chat info";
    }
}

template <>
void executeCommand<Commands::DeleteChat>(const CommandData& data) {
    // Again, not implemented for now
}

}  // namespace

int main(const int argc, const char** argv) {
    TgBot_AbslLogInit();
    CommandData data = {argc, argv, nullptr};

    auto dbImpl = TgBotDatabaseImpl::getInstance();
    if (!dbImpl->loadDBFromConfig()) {
        LOG(ERROR) << "Failed to load database";
        return EXIT_FAILURE;
    }
    if (argc < 2) {
        executeCommand<Commands::Help>(data);
        return EXIT_SUCCESS;
    }
    const std::string command = argv[1];
    const int argcPartial = argc - 2;
    const char** argvPartial = argv += 2;
    data = {argcPartial, argvPartial, dbImpl};

    DLOG(INFO) << "Executing command: " << command;
    if (command == "dump") {
        executeCommand<Commands::Dump>(data);
    } else if (command == "delete_media") {
        executeCommand<Commands::DeleteMedia>(data);
    } else if (command == "add_chat") {
        executeCommand<Commands::AddChat>(data);
    } else if (command == "delete_chat") {
        executeCommand<Commands::DeleteChat>(data);
    } else {
        LOG(ERROR) << "Unknown command: " << command;
    }
    dbImpl->unloadDatabase();
}
