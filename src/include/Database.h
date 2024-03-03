#pragma once

#include <Logging.h>
#include <TgBotDB.pb.h>
#include <Types.h>
#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>

#include <mutex>
#include <optional>
#include <stdexcept>

#include "../popen_wdt/popen_wdt.hpp"

static inline const char kDatabaseFile[] = "tgbot.pb";

inline std::filesystem::path getDatabaseFile() {
    return getSrcRoot() / std::string(kDatabaseFile);
}

namespace database {

using TgBot::Bot;
using TgBot::Message;
using TgBot::User;
using ::google::protobuf::RepeatedField;
using ::tgbot::proto::Database;
using ::tgbot::proto::PersonList;

class ProtoDatabase {
    void _addToDatabase(const Bot& bot, const Message::Ptr& message,
                        RepeatedField<UserId>* list, const std::string& name);
    void _removeFromDatabase(const Bot& bot, const Message::Ptr& message,
                             RepeatedField<UserId>* list, const std::string& name);
    bool rejectUid(const Bot& bot, const User::Ptr& user) const;
    std::optional<int> findByUid(const RepeatedField<UserId>* list, const UserId uid) const;

   public:
    std::string name;
    RepeatedField<UserId>* list;
    const ProtoDatabase* other;

    void addToDatabase(const Bot& bot, const Message::Ptr& message) {
        _addToDatabase(bot, message, list, name);
    }
    void removeFromDatabase(const Bot& bot, const Message::Ptr& message) {
        _removeFromDatabase(bot, message, list, name);
    }
    bool exists(const UserId id) const {
        return findByUid(list, id).has_value();
    }
};

struct DatabaseWrapper {
    void load(bool withSync = false);
    void save(void) const;
    UserId maybeGetOwnerId() const;

    Database* getMainDatabase(void) {
        return &protodb;
    }

    DatabaseWrapper() = default;
    ~DatabaseWrapper() {
        save();
    }

   private:
    bool warnNoLoaded(const char* func) const;
    Database protodb;
    std::string fname;
    bool loaded = false;
    std::once_flag once;
};

extern ProtoDatabase whitelist;
extern ProtoDatabase blacklist;
extern DatabaseWrapper DBWrapper;

};  // namespace database
