#pragma once

#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>

#include "conf/conf.h"

namespace database {

using ::TgBot::Bot;
using ::TgBot::Message;

using dblist_getter_t = std::function<UserId *(struct config_data *data)>;
using internal_op_t = std::function<void(const Bot &bot, const Message::Ptr &message,
                                         const dblist_getter_t getter, const char *listname)>;
class DBOperationsBase {
    internal_op_t add_, remove_;
    dblist_getter_t getter_;
    const char *name_;

   public:
    void add(const Bot &bot, const Message::Ptr &message) {
        add_(bot, message, getter_, name_);
    }
    void remove(const Bot &bot, const Message::Ptr &message) {
        remove_(bot, message, getter_, name_);
    }
    DBOperationsBase(internal_op_t add, internal_op_t remove, dblist_getter_t getter, const char *name)
        : add_(add), remove_(remove), getter_(getter), name_(name) {}
};

extern DBOperationsBase blacklist;
extern DBOperationsBase whitelist;
extern TgBotConfig config;

};  // namespace database
