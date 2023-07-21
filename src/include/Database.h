#pragma once

#include <conf/conf.h>
#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>

#include "NamespaceImport.h"

namespace database {

class DBOperationsBase;

using ::Bot;
using ::Message;
using dblist_getter_t = std::function<UserId *(struct config_data *data)>;
using internal_op_t = std::function<void(const DBOperationsBase *thisptr, const Bot &bot,
                                         const Message::Ptr &message, const dblist_getter_t getter,
                                         const char *listname)>;

class DBOperationsBase {
    internal_op_t add_, remove_;

   public:
    const char *name;
    dblist_getter_t getter;
    const DBOperationsBase *other;

    void add(const Bot &bot, const Message::Ptr &message) {
        add_(this, bot, message, getter, name);
    }
    void remove(const Bot &bot, const Message::Ptr &message) {
        remove_(this, bot, message, getter, name);
    }
    DBOperationsBase(internal_op_t add, internal_op_t remove, dblist_getter_t getter_,
                     const char *name_, const DBOperationsBase *other_)
        : add_(add), remove_(remove), getter(getter_), name(name_), other(other_) {}
};

extern DBOperationsBase blacklist;
extern DBOperationsBase whitelist;
extern TgBotConfig config;

};  // namespace database
