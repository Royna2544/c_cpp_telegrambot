#include <BotReplyMessage.h>
#include <Database.h>

#include <type_traits>

namespace database {

DatabaseWrapper db("tgbot.pb");

static std::string appendListName(const std::string& op, const int64_t id, const std::string& name) {
    return std::string("User") + ' ' + std::to_string(id) + ' ' + op + ' ' + name;
}

std::optional<int> ProtoDatabase::findByUid(const RepeatedField<int64_t>* list,
                                            const int64_t uid) {
    for (auto it = list->begin(); it != list->end(); ++it) {
        if (list->Get(std::distance(list->begin(), it)) == uid) {
            return std::distance(list->begin(), it);
        }
    }
    return std::nullopt;
}

bool ProtoDatabase::rejectUid(const Bot& bot, const User::Ptr& user, const int64_t id) {
    if (bot.getApi().getMe()->id == id) return true;
    if (db->has_ownerid() && db->ownerid() == id) return true;
    if (user->isBot) return true;
    return false;
}

void ProtoDatabase::_addToDatabase(const Bot& bot, const Message::Ptr& message,
                                   RepeatedField<int64_t>* list, const std::string& name) {
    if (message->replyToMessage && message->replyToMessage->from) {
        int64_t id = message->replyToMessage->from->id;
        if (rejectUid(bot, message->replyToMessage->from, id)) return;
        if (findByUid(list, id)) {
            bot_sendReplyMessage(bot, message, appendListName("already in", id, name));
            return;
        }
        if (findByUid(other->list, id)) {
            bot_sendReplyMessage(bot, message, appendListName("already in", id, other->name));
            return;
        }
        *list->Add() = id;
        bot_sendReplyMessage(bot, message, appendListName("added to", id, name));
    } else {
        bot_sendReplyMessage(bot, message, "Reply to a user.");
    }
}
void ProtoDatabase::_removeFromDatabase(const Bot& bot, const Message::Ptr& message,
                                        RepeatedField<int64_t>* list, const std::string& name) {
    if (message->replyToMessage && message->replyToMessage->from) {
        int64_t id = message->replyToMessage->from->id;
        if (rejectUid(bot, message->replyToMessage->from, id)) return;
        auto idx = findByUid(list, id);
        if (idx.has_value()) {
            list->erase(list->begin() + *idx);
            bot_sendReplyMessage(bot, message, appendListName("removed from", id, name));
        } else
            bot_sendReplyMessage(bot, message, appendListName("not found", id, name));
    } else {
        bot_sendReplyMessage(bot, message, "Reply to a user.");
    }
}

ProtoDatabase whitelist = {
    "whitelist",
    db->mutable_whitelist()->mutable_id(),
    &blacklist,
};
ProtoDatabase blacklist = {
    "blacklist",
    db->mutable_blacklist()->mutable_id(),
    &whitelist,
};

}  // namespace database
