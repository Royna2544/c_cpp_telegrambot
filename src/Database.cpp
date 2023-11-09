#include <BotReplyMessage.h>
#include <Database.h>
#include <utils/libutils.h>

namespace database {

DatabaseWrapper db(getSrcRoot() + "/tgbot.pb");

static std::string appendListName(const std::string& op, const UserId id, const std::string& name) {
    return std::string("User") + ' ' + std::to_string(id) + ' ' + op + ' ' + name;
}

std::optional<int> ProtoDatabase::findByUid(const RepeatedField<UserId>* list,
                                            const UserId uid) const {
    for (auto it = list->begin(); it != list->end(); ++it) {
        if (list->Get(std::distance(list->begin(), it)) == uid) {
            return std::distance(list->begin(), it);
        }
    }
    return std::nullopt;
}

bool ProtoDatabase::rejectUid(const Bot& bot, const User::Ptr& user) const {
    if (bot.getApi().getMe()->id == user->id) return true;
    if (db->has_ownerid() && db->ownerid() == user->id) return true;
    if (user->isBot) return true;
    return false;
}

void ProtoDatabase::_addToDatabase(const Bot& bot, const Message::Ptr& message,
                                   RepeatedField<UserId>* list, const std::string& name) {
    if (message->replyToMessage && message->replyToMessage->from) {
        UserId id = message->replyToMessage->from->id;
        if (rejectUid(bot, message->replyToMessage->from)) return;
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
                                        RepeatedField<UserId>* list, const std::string& name) {
    if (message->replyToMessage && message->replyToMessage->from) {
        UserId id = message->replyToMessage->from->id;
        if (rejectUid(bot, message->replyToMessage->from)) return;
        auto idx = findByUid(list, id);
        if (idx.has_value()) {
            list->erase(list->begin() + *idx);
            bot_sendReplyMessage(bot, message, appendListName("removed from", id, name));
        } else
            bot_sendReplyMessage(bot, message, appendListName("not found in", id, name));
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
