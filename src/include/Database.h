#pragma once

#include <TgBotDB.pb.h>
#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>

#include <fstream>
#include <optional>

#include "NamespaceImport.h"

namespace database {

using ::Bot;
using ::Message;
using ::google::protobuf::RepeatedField;
using ::TgBot::User;
using ::tgbot::proto::Database;
using ::tgbot::proto::PersonList;

class ProtoDatabase {
    void _addToDatabase(const Bot& bot, const Message::Ptr& message,
                        RepeatedField<int64_t>* list, const std::string& name);
    void _removeFromDatabase(const Bot& bot, const Message::Ptr& message,
                             RepeatedField<int64_t>* list, const std::string& name);
    bool rejectUid(const Bot& bot, const User::Ptr& user, const int64_t id);
    std::optional<int> findByUid(const RepeatedField<int64_t>* list, const int64_t uid);

   public:
    std::string name;
    RepeatedField<int64_t>* list;
    const ProtoDatabase* other;

    void addToDatabase(const Bot& bot, const Message::Ptr& message) {
        _addToDatabase(bot, message, list, name);
    }
    void removeFromDatabase(const Bot& bot, const Message::Ptr& message) {
        _removeFromDatabase(bot, message, list, name);
    }
    bool exists(const int64_t id) {
        return findByUid(list, id).has_value();
    }
};

struct DatabaseWrapper {
    DatabaseWrapper(const std::string fname) : fname(fname) {
        std::fstream input(fname, std::ios::in | std::ios::binary);
        if (input) {
            protodb.ParseFromIstream(&input);
        }
    }
    ~DatabaseWrapper() {
        save();
    }
    void save(void) {
        std::fstream output(fname, std::ios::out | std::ios::trunc | std::ios::binary);
        if (output) {
            protodb.SerializeToOstream(&output);
        }
    }
    Database* operator->(void) {
        return &protodb;
    }
    std::optional<int64_t> maybeGetOwnerId() {
        if (protodb.has_ownerid())
            return protodb.ownerid();
        else
            return std::nullopt;
    }

   private:
    Database protodb;
    std::string fname;
};

extern ProtoDatabase whitelist;
extern ProtoDatabase blacklist;
extern DatabaseWrapper db;

};  // namespace database
