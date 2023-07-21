#include <BotReplyMessage.h>
#include <Database.h>

namespace database {
namespace defaultimpl {

static bool isInDB(const UserId *listdata, const UserId id) {
    for (int i = 0; i < DATABASE_LIST_BUFSIZE; i++) {
        if (listdata[i] == id) return true;
    }
    return false;
}

static void addToDBList(const DBOperationsBase *thisptr, const Bot &bot, const Message::Ptr &message,
                        const dblist_getter_t getter, const char *listname) {
    struct config_data data;
    UserId *listdata = getter(&data);
    if (message->replyToMessage && message->replyToMessage->from) {
        if (bot.getApi().getMe()->id == message->replyToMessage->from->id) return;
        config.loadFromFile(&data);
        if (data.owner_id == message->replyToMessage->from->id) {
            bot_sendReplyMessage(bot, message, std::string() + "Cannot add owner in " + listname);
            return;
        }
        if (isInDB(thisptr->other->getter(&data), message->replyToMessage->from->id)) {
            bot_sendReplyMessage(bot, message, std::string() + "Remove user from " + thisptr->other->name + " first");
            return;
        }
        for (int i = 0; i < DATABASE_LIST_BUFSIZE; i++) {
            if (listdata[i] == message->replyToMessage->from->id) {
                bot_sendReplyMessage(bot, message, std::string() + "User already in " + listname);
                return;
            }
            if (listdata[i] == 0) {
                listdata[i] = message->replyToMessage->from->id;
                bot_sendReplyMessage(bot, message, std::string() + "User added to " + listname);
                config.storeToFile(data);
                return;
            }
        }
        bot_sendReplyMessage(bot, message, "Out of buffer");
    } else {
        bot_sendReplyMessage(bot, message, "Reply to a user");
    }
}
static void removeFromDBList(const DBOperationsBase *thisptr, const Bot &bot,
                             const Message::Ptr &message,
                             const dblist_getter_t getter, const char *listname) {
    struct config_data data;
    UserId *listdata = getter(&data);
    if (message->replyToMessage && message->replyToMessage->from) {
        bool changed = false;
        config.loadFromFile(&data);
        int tmp[DATABASE_LIST_BUFSIZE] = {
            0,
        };
        for (int i = 0; i < DATABASE_LIST_BUFSIZE; i++) {
            if (listdata[i] == message->replyToMessage->from->id) {
                bot_sendReplyMessage(bot, message, std::string() + "User removed from " + listname);
                changed = true;
                continue;
            } else {
                tmp[i] = listdata[i];
            }
        }
        if (changed) {
            memcpy(listdata, tmp, sizeof(tmp));
            config.storeToFile(data);
        } else {
            bot_sendReplyMessage(bot, message, std::string() + "User not in " + listname);
        }
    } else {
        bot_sendReplyMessage(bot, message, "Reply to a user");
    }
}

};  // namespace defaultimpl

DBOperationsBase blacklist = {
    defaultimpl::addToDBList,
    defaultimpl::removeFromDBList,
    [](struct config_data *data) -> UserId * { return &data->blacklist[0]; },
    "blacklist",
    &whitelist,
};
DBOperationsBase whitelist = {
    defaultimpl::addToDBList,
    defaultimpl::removeFromDBList,
    [](struct config_data *data) -> UserId * { return &data->whitelist[0]; },
    "whitelist",
    &blacklist,
};

TgBotConfig config("tgbot.dat");

};  // namespace database
