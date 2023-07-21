#include <BotReplyMessage.h>
#include <Database.h>

namespace database {
namespace defaultimpl {

static void addToDBList(const Bot &bot, const Message::Ptr &message,
                        const dblist_getter_t getter, const char *listname) {
    struct config_data data;
    UserId *listdata = getter(&data);
    if (message->replyToMessage && message->replyToMessage->from) {
        config.loadFromFile(&data);
        if (data.owner_id == message->replyToMessage->from->id) {
            bot_sendReplyMessage(bot, message, std::string() + "Cannot add owner in " + listname);
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
static void removeFromDBList(const Bot &bot, const Message::Ptr &message,
                             const dblist_getter_t getter, const char *listname) {
    struct config_data data;
    UserId *listdata = getter(&data);
    if (message->replyToMessage && message->replyToMessage->from) {
        config.loadFromFile(&data);
        int tmp[DATABASE_LIST_BUFSIZE] = {
            0,
        };
        for (int i = 0; i < DATABASE_LIST_BUFSIZE; i++) {
            if (listdata[i] == message->replyToMessage->from->id) {
                bot_sendReplyMessage(bot, message, std::string() + "User removed from " + listname);
                continue;
            } else {
                tmp[i] = listdata[i];
            }
        }
        memcpy(listdata, tmp, sizeof(tmp));
        config.storeToFile(data);
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
};
DBOperationsBase whitelist = {
    defaultimpl::addToDBList,
    defaultimpl::removeFromDBList,
    [](struct config_data *data) -> UserId * { return &data->whitelist[0]; },
    "whitelist",
};

TgBotConfig config("tgbot.dat");

};  // namespace database
