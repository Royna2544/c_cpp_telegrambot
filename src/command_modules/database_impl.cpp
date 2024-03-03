#include <functional>
#include <optional>

#include <BotReplyMessage.h>
#include <Database.h>
#include <ExtArgs.h>

#include "CommandModule.h"
#include "tgbot/tools/StringTools.h"

using database::blacklist;
using database::ProtoDatabase;
using database::whitelist;

struct CommandModule cmd_addblacklist {
    .enforced = true,
    .name = "addblacklist",
    .fn = std::bind(&ProtoDatabase::addToDatabase, blacklist, std::placeholders::_1, std::placeholders::_2)
};

struct CommandModule cmd_rmblacklist {
    .enforced = true,
    .name = "rmblacklist",
    .fn = std::bind(&ProtoDatabase::removeFromDatabase, blacklist, std::placeholders::_1, std::placeholders::_2)
};

struct CommandModule cmd_addwhitelist {
    .enforced = true,
    .name = "addwhitelist",
    .fn = std::bind(&ProtoDatabase::addToDatabase, whitelist, std::placeholders::_1, std::placeholders::_2)
};

struct CommandModule cmd_rmwhitelist {
    .enforced = true,
    .name = "rmwhitelist",
    .fn = std::bind(&ProtoDatabase::removeFromDatabase, whitelist, std::placeholders::_1, std::placeholders::_2)
};

static void saveIdFn(const Bot& bot, const Message::Ptr& message) {
    const auto nonMutableMediaDB = database::DBWrapper.getMediaDatabase();

    if (hasExtArgs(message)) {
        std::string names;
        std::optional<std::string> fileId;
        parseExtArgs(message, names);

        if (message->replyToMessage) {
            if (const auto it = message->replyToMessage->animation; it) {
                fileId = it->fileId;
            } else if (const auto it = message->replyToMessage->sticker; it) {
                fileId = it->fileId;
            }
        }
        if (fileId) {
            if (nonMutableMediaDB->entries().size() > 0) {
                for (size_t i = 0; i < nonMutableMediaDB->entries_size(); ++i) {
                    const auto& it = nonMutableMediaDB->entries(i);
                    if (it.has_telegrammediaid() && fileId == it.telegrammediaid()) {
                        bot_sendReplyMessage(bot, message, "FileId already exists on MediaDatabase");
                        return;
                    }
                }
            }
            const auto namevec = StringTools::split(names, '/');
            auto ent = nonMutableMediaDB->add_entries();
            std::stringstream ss;

            ent->set_telegrammediaid(*fileId);
            ss << "Media " << *fileId << " (fileid) added" << std::endl;
            ss << "With names:" << std::endl;
            for (const auto &names : namevec) {
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

struct CommandModule cmd_saveid {
    .enforced = true,
    .name = "saveid",
    .fn = saveIdFn,
};
