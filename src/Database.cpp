#include <BotReplyMessage.h>
#include <Database.h>

namespace database {

std::optional<int> ProtoDatabaseBase::findByUid(
    const RepeatedField<UserId>* list, const UserId uid) const {
    for (auto it = list->begin(); it != list->end(); ++it) {
        auto distance = std::distance(list->begin(), it);
        if (list->Get(distance) == uid) {
            return distance;
        }
    }
    return std::nullopt;
}

void ProtoDatabaseBase::_addToDatabase(const Message::Ptr& message,
                                       RepeatedField<UserId>* list) {
    if (message->replyToMessage && message->replyToMessage->from) {
        const User::Ptr user = message->replyToMessage->from;
        const UserId id = user->id;
        do {
            if (rejectUid(user)) break;
            if (findByUid(list, id)) {
                onAlreadyExist(message, user, this);
                break;
            }
            if (!other.expired()) {
                if (const auto it = other.lock(); findByUid(it->list, id)) {
                    onAlreadyExist(message, user, it.get());
                    break;
                }
            }
            *list->Add() = id;
            onAdded(message, user, this);
        } while (false);
    } else {
        onUserNotFoundOnMessage(message);
    }
}
void ProtoDatabaseBase::_removeFromDatabase(const Message::Ptr& message,
                                            RepeatedField<UserId>* list) {
    if (message->replyToMessage && message->replyToMessage->from) {
        const User::Ptr user = message->replyToMessage->from;
        const UserId id = user->id;
        do {
            if (rejectUid(user)) break;

            const auto idx = findByUid(list, id);
            if (idx.has_value()) {
                list->erase(list->begin() + *idx);
                onRemoved(message, user, this);
            } else
                onNotFound(message, user, this);
        } while (false);
    } else {
        onUserNotFoundOnMessage(message);
    }
}

bool ProtoDatabase::rejectUid(const User::Ptr& user) const {
    if (_bot.getApi().getMe()->id == user->id) return true;
    if (DBWrapper.maybeGetOwnerId() == user->id) return true;
    if (user->isBot) return true;
    return false;
}

void ProtoDatabase::onAlreadyExist(const Message::Ptr& message,
                                   const User::Ptr& who,
                                   const ProtoDatabaseBase* which) const {
    std::string text = appendListName("already in", who, which);
    if (which != this) {
        text += " Remove the user from ";
        text += which->name;
        text += " first.";
    }
    bot_sendReplyMessage(_bot, message, text);
}
void ProtoDatabase::onAdded(const Message::Ptr& message, const User::Ptr& who,
                            const ProtoDatabaseBase* which) const {
    bot_sendReplyMessage(_bot, message, appendListName("added to", who, which));
}
void ProtoDatabase::onNotFound(const Message::Ptr& message,
                               const User::Ptr& who,
                               const ProtoDatabaseBase* which) const {
    bot_sendReplyMessage(_bot, message,
                         appendListName("not found in", who, which));
}
void ProtoDatabase::onRemoved(const Message::Ptr& message, const User::Ptr& who,
                              const ProtoDatabaseBase* which) const {
    bot_sendReplyMessage(_bot, message,
                         appendListName("removed from", who, which));
}
void ProtoDatabase::onUserNotFoundOnMessage(const Message::Ptr& message) const {
    bot_sendReplyMessage(_bot, message, "Reply to a user.");
}
std::string ProtoDatabase::appendListName(const std::string& op,
                                          const User::Ptr from,
                                          const ProtoDatabaseBase* what) {
    std::stringstream ss;
    ss << "User " << from->id << ' ' << op << ' ' << what->name << '.';
    return ss.str();
}

}  // namespace database
