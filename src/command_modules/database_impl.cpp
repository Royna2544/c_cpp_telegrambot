#include <BotReplyMessage.h>
#include <ExtArgs.h>
#include <tgbot/tools/StringTools.h>

#include <DatabaseBot.hpp>
#include <database/DatabaseBase.hpp>
#include <optional>

#include "CommandModule.h"

namespace {
bool checkDatabaseCommand(const Bot& bot, const Message::Ptr& message) {
    if (!(message->replyToMessage && message->replyToMessage->from)) {
        bot_sendReplyMessage(bot, message, "Reply to a user's message");
        return false;
    }
    return true;
}
}  // namespace

template <DatabaseBase::ListType type>
void handleAddUser(const Bot& bot, const Message::Ptr& message) {
    auto& base = DefaultBotDatabase::getInstance();
    if (!checkDatabaseCommand(bot, message)) {
        return;
    }
    auto res = base.addUserToList(type, message->replyToMessage->from->id);
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
    auto& base = DefaultBotDatabase::getInstance();
    if (!checkDatabaseCommand(bot, message)) {
        return;
    }
    auto res = base.removeUserFromList(type, message->replyToMessage->from->id);
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

struct CommandModule cmd_addblacklist(
    "addblacklist", "Add blacklisted user to the database",
    CommandModule::Flags::Enforced | CommandModule::Flags::HideDescription,
    [](const Bot& bot, const Message::Ptr& message) {
        handleAddUser<DatabaseBase::ListType::BLACKLIST>(bot, message);
    });

struct CommandModule cmd_rmblacklist(
    "rmblacklist", "Remove blacklisted user from the database",
    CommandModule::Flags::Enforced | CommandModule::Flags::HideDescription,
    handleRemoveUser<DatabaseBase::ListType::BLACKLIST>);

struct CommandModule cmd_addwhitelist(
    "addwhitelist", "Add whitelisted user to the database",
    CommandModule::Flags::Enforced | CommandModule::Flags::HideDescription,
    handleAddUser<DatabaseBase::ListType::WHITELIST>);

struct CommandModule cmd_rmwhitelist(
    "rmwhitelist", "Remove whitelisted user from the database",
    CommandModule::Flags::Enforced | CommandModule::Flags::HideDescription,
    handleRemoveUser<DatabaseBase::ListType::WHITELIST>);

static void saveIdFn(const Bot& bot, const Message::Ptr& message) {
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
            const auto namevec = StringTools::split(names, '/');
            auto const& backend = DefaultBotDatabase::getInstance();
            info.mediaId = fileId.value();
            info.mediaUniqueId = fileUniqueId.value();

            std::stringstream ss;
            ss << "Media " << *fileUniqueId << " (fileUniqueId) added"
               << std::endl;
            ss << "With names:" << std::endl;
            for (const auto& names : namevec) {
                info.names = names;
                ss << "- " << names << std::endl;
            }
            if (backend.addMediaInfo(info)) {
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

// clang-format off
struct CommandModule cmd_saveid("saveid",
    "Save media to MediaDatabase for later use",
    CommandModule::Flags::Enforced | CommandModule::Flags::HideDescription, saveIdFn);