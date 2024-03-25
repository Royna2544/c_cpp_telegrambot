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

}  // namespace database
