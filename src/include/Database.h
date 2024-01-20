#pragma once

#include <Logging.h>
#include <TgBotDB.pb.h>
#include <Types.h>
#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>

#include <fstream>
#include <optional>

#include "NamespaceImport.h"

static inline const char kDatabaseFile[] = "tgbot.pb";

namespace database {

using ::Bot;
using ::Message;
using ::google::protobuf::RepeatedField;
using ::TgBot::User;
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
    ~DatabaseWrapper() {
        save();
    }
    void load(const std::string& _fname) {
        fname = _fname;
        std::fstream input(fname, std::ios::in | std::ios::binary);
        assert(input);
        protodb.ParseFromIstream(&input);
        loaded = true;
    }
    void save(void) const {
        if (warnNoLoaded(__func__)) {
            std::fstream output(fname, std::ios::out | std::ios::trunc | std::ios::binary);
            assert(output);
            protodb.SerializeToOstream(&output);
        }
    }
    Database* operator->(void) {
        return &protodb;
    }
    UserId maybeGetOwnerId() const {
        if (warnNoLoaded(__func__) && protodb.has_ownerid())
            return protodb.ownerid();
        else
            return -1;
    }

   private:
    bool warnNoLoaded(const char* func) const {
        if (!loaded) {
            LOG_W("Database not loaded! Called function: '%s'", func);
        }
        return loaded;
    }
    Database protodb;
    std::string fname;
    bool loaded = false;
};

extern ProtoDatabase whitelist;
extern ProtoDatabase blacklist;
extern DatabaseWrapper db;

};  // namespace database
