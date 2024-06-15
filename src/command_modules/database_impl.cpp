#include <BotReplyMessage.h>
#include <ExtArgs.h>
#include <tgbot/tools/StringTools.h>

#include <database/bot/TgBotDatabaseImpl.hpp>
#include <optional>

#include "CommandModule.h"
#include "tgbot/types/Message.h"

template <DatabaseBase::ListType type>
void handleAddUser(const Bot& bot, const Message::Ptr& message) {
    auto base = TgBotDatabaseImpl::getInstance();
    auto res = base->addUserToList(type, message->replyToMessage->from->id);
    std::string text;
    switch (res) {
        case DatabaseBase::ListResult::OK:
            text = "User added to list";
            break;
        case DatabaseBase::ListResult::ALREADY_IN_LIST:
            text = "User is already in the list";
            break;
        case DatabaseBase::ListResult::ALREADY_IN_OTHER_LIST:
            text = "User is already in another list";
            break;
        case DatabaseBase::ListResult::BACKEND_ERROR:
            text = "Backend error";
            break;
        default:
            LOG(ERROR) << "Unhandled result type " << static_cast<int>(res);
            text = "Unknown error";
    }
    bot_sendReplyMessage(bot, message, text);
}

template <DatabaseBase::ListType type>
void handleRemoveUser(const Bot& bot, const Message::Ptr& message) {
    auto base = TgBotDatabaseImpl::getInstance();
    auto res =
        base->removeUserFromList(type, message->replyToMessage->from->id);
    std::string text;
    switch (res) {
        case DatabaseBase::ListResult::OK:
            text = "User removed from list";
            break;
        case DatabaseBase::ListResult::NOT_IN_LIST:
            text = "User is not in the list";
            break;
        case DatabaseBase::ListResult::ALREADY_IN_OTHER_LIST:
            text = "User is already in another list";
            break;
        case DatabaseBase::ListResult::BACKEND_ERROR:
            text = "Backend error";
            break;
        default:
            LOG(ERROR) << "Unhandled result type " << static_cast<int>(res);
            text = "Unknown error";
    }
    bot_sendReplyMessage(bot, message, text);
}
namespace {

constexpr std::string_view whitelist = "whitelist";
constexpr std::string_view blacklist = "blacklist";
constexpr std::string_view add = "add";
constexpr std::string_view remove = "remove";

void handleDatabaseCmd(const Bot& bot, const Message::Ptr& message) {
    if (!(message->replyToMessage && message->replyToMessage->from)) {
        bot_sendReplyMessage(bot, message, "Reply to a user's message");
    } else if (hasExtArgs(message)) {
        const auto args = StringTools::split(parseExtArgs(message), ' ');
        std::function<void(const Bot&, const Message::Ptr&)> handler;
        std::string errorMessage;

        if (args.size() != 2) {
            errorMessage =
                "Invalid arguments supplied. "
                "Example: /database whitelist add";
        } else {
            if (args[0] == whitelist) {
                if (args[1] == add) {
                    handler = handleAddUser<DatabaseBase::ListType::WHITELIST>;
                } else if (args[1] == remove) {
                    handler =
                        handleRemoveUser<DatabaseBase::ListType::WHITELIST>;
                } else {
                    errorMessage = "Invalid argument for [operation]";
                }
            } else if (args[0] == blacklist) {
                if (args[1] == add) {
                    handler = handleAddUser<DatabaseBase::ListType::BLACKLIST>;
                } else if (args[1] == remove) {
                    handler =
                        handleRemoveUser<DatabaseBase::ListType::BLACKLIST>;
                } else {
                    errorMessage = "Invalid argument for [operation]";
                }
            } else {
                errorMessage = "Invalid argument for [listtype]";
            }
        }
        if (handler) {
            handler(bot, message);
        } else {
            bot_sendReplyMessage(bot, message, errorMessage);
        }
    }
};

void handleSaveIdCmd(const Bot& bot, const Message::Ptr& message) {
    if (hasExtArgs(message)) {
        std::string names;
        std::optional<std::string> fileId;
        std::optional<std::string> fileUniqueId;
        parseExtArgs(message, names);

        if (message->replyToMessage) {
            if (const auto it = message->replyToMessage->animation; it) {
                fileId = it->fileId;
                fileUniqueId = it->fileUniqueId;
            } else if (const auto it = message->replyToMessage->sticker; it) {
                fileId = it->fileId;
                fileUniqueId = it->fileUniqueId;
            }
        }
        if (fileId && fileUniqueId) {
            DatabaseBase::MediaInfo info{};
            auto const& backend = TgBotDatabaseImpl::getInstance();
            info.mediaId = fileId.value();
            info.mediaUniqueId = fileUniqueId.value();

            std::stringstream ss;
            ss << "Media " << *fileUniqueId << " (fileUniqueId) added"
               << std::endl;
            ss << "With name:" << names << std::endl;
            info.names = names;
            if (backend->addMediaInfo(info)) {
                bot_sendReplyMessage(bot, message, ss.str());
            } else {
                bot_sendReplyMessage(bot, message,
                                     "Media is already in database");
            }
        } else {
            bot_sendReplyMessage(bot, message, "Reply to a GIF or sticker");
        }
    } else {
        bot_sendReplyMessage(bot, message, "Send names sperated by '/'");
    }
}

}  // namespace

void loadcmd_database(CommandModule& module) {
    module.command = "database";
    module.description = "Run database commands";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = handleDatabaseCmd;
}

void loadcmd_saveid(CommandModule& module) {
    module.command = "saveid";
    module.description =
        "Save database information about media for later retrieval";
    module.flags =
        CommandModule::Flags::Enforced | CommandModule::Flags::HideDescription;
    module.fn = handleSaveIdCmd;
}
