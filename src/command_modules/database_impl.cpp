#include <BotReplyMessage.h>
#include <Database.h>
#include <ExtArgs.h>

#include <optional>

#include "CommandModule.h"
#include "tgbot/tools/StringTools.h"

static database::DatabaseWrapperImplObj& getDatabaseWrapper() {
    return database::DatabaseWrapperImplObj::getInstance();
}

struct CommandModule cmd_addblacklist(
    "addblacklist", "Add blacklisted user to the database",
    CommandModule::Flags::Enforced | CommandModule::Flags::HideDescription,
    [](const Bot& bot, const Message::Ptr& message) {
        getDatabaseWrapper().blacklist->addToDatabase(message);
    });

struct CommandModule cmd_rmblacklist(
    "rmblacklist", "Remove blacklisted user from the database",
    CommandModule::Flags::Enforced | CommandModule::Flags::HideDescription,
    [](const Bot& bot, const Message::Ptr& message) {
        getDatabaseWrapper().blacklist->removeFromDatabase(message);
    });

struct CommandModule cmd_addwhitelist(
    "addwhitelist", "Add whitelisted user to the database",
    CommandModule::Flags::Enforced | CommandModule::Flags::HideDescription,
    [](const Bot& bot, const Message::Ptr& message) {
        getDatabaseWrapper().whitelist->addToDatabase(message);
    });

struct CommandModule cmd_rmwhitelist(
    "rmwhitelist", "Remove whitelisted user from the database",
    CommandModule::Flags::Enforced | CommandModule::Flags::HideDescription,
    [](const Bot& bot, const Message::Ptr& message) {
        getDatabaseWrapper().whitelist->removeFromDatabase(message);
    });

static void saveIdFn(const Bot& bot, const Message::Ptr& message) {
    const auto mutableMediaDB =
        getDatabaseWrapper().protodb.mutable_mediatonames();

    if (hasExtArgs(message)) {
        std::string names;
        std::optional<std::string> fileId, fileUniqueId;
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
        if (fileId) {
            if (const auto mediaSize = mutableMediaDB->size(); mediaSize > 0) {
                for (int i = 0; i < mediaSize; ++i) {
                    const auto& it = mutableMediaDB->Get(i);
                    if (it.has_telegrammediauniqueid() &&
                        fileUniqueId == it.telegrammediauniqueid()) {
                        bot_sendReplyMessage(
                            bot, message,
                            "FileUniqueId already exists on MediaDatabase");
                        return;
                    }
                }
            }
            const auto namevec = StringTools::split(names, '/');
            auto ent = mutableMediaDB->Add();
            std::stringstream ss;

            ent->set_telegrammediaid(*fileId);
            ent->set_telegrammediauniqueid(*fileUniqueId);
            ss << "Media " << *fileUniqueId << " (fileUniqueId) added"
               << std::endl;
            ss << "With names:" << std::endl;
            for (const auto& names : namevec) {
                *ent->add_names() = names;
                ss << "- " << names << std::endl;
            }
            bot_sendReplyMessage(bot, message, ss.str());
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