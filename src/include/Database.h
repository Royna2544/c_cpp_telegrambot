#pragma once

#include <TgBotDB.pb.h>
#include <Types.h>
#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>

#include <filesystem>
#include <libos/libfs.hpp>
#include <mutex>
#include <optional>

#include "BotClassBase.h"
#include "InstanceClassBase.hpp"
#include "initcalls/Initcall.hpp"
#include "internal/_class_helper_macros.h"

constexpr std::string_view kDatabaseFile = "tgbot.pb";

namespace database {

using ::google::protobuf::RepeatedField;
using TgBot::Bot;
using TgBot::Message;
using TgBot::User;
using ::tgbot::proto::Database;
using ::tgbot::proto::PersonList;

class ProtoDatabaseBase {
    void _addToDatabase(const Message::Ptr& message,
                        RepeatedField<UserId>* list);
    void _removeFromDatabase(const Message::Ptr& message,
                             RepeatedField<UserId>* list);
    std::optional<int> findByUid(const RepeatedField<UserId>* list,
                                 const UserId uid) const;

   public:
    const char* name;

   protected:
    RepeatedField<UserId>* list;
    std::weak_ptr<ProtoDatabaseBase> other;

   public:
    explicit ProtoDatabaseBase(const char* _name, RepeatedField<UserId>* _list)
        : name(_name), list(_list) {}
    virtual ~ProtoDatabaseBase() {}

    virtual bool rejectUid(const User::Ptr& user) const {
        if (user->isBot) return true;
        return false;
    }
    virtual void onAlreadyExist(const Message::Ptr& message,
                                const User::Ptr& who,
                                const ProtoDatabaseBase* which) const = 0;
    virtual void onAdded(const Message::Ptr& message, const User::Ptr& who,
                         const ProtoDatabaseBase* which) const = 0;
    virtual void onNotFound(const Message::Ptr& message, const User::Ptr& who,
                            const ProtoDatabaseBase* which) const = 0;
    virtual void onRemoved(const Message::Ptr& message, const User::Ptr& who,
                           const ProtoDatabaseBase* which) const = 0;
    virtual void onUserNotFoundOnMessage(const Message::Ptr& message) const = 0;

    void setOtherProtoDatabaseBase(
        const std::weak_ptr<ProtoDatabaseBase> _other) {
        other = _other;
    }

    void addToDatabase(const Message::Ptr& message) {
        _addToDatabase(message, list);
    }
    void removeFromDatabase(const Message::Ptr& message) {
        _removeFromDatabase(message, list);
    }
    bool exists(const UserId id) const {
        return findByUid(list, id).has_value();
    }
};

struct ProtoDatabase : ProtoDatabaseBase, BotClassBase {
    ProtoDatabase(const Bot& bot, const char* _name,
                  RepeatedField<UserId>* _list)
        : ProtoDatabaseBase(_name, _list), BotClassBase(bot) {}
    ~ProtoDatabase() override = default;

    void onAlreadyExist(const Message::Ptr& message, const User::Ptr& who,
                        const ProtoDatabaseBase* which) const override;
    void onAdded(const Message::Ptr& message, const User::Ptr& who,
                 const ProtoDatabaseBase* which) const override;
    void onNotFound(const Message::Ptr& message, const User::Ptr& who,
                    const ProtoDatabaseBase* which) const override;
    void onRemoved(const Message::Ptr& message, const User::Ptr& who,
                   const ProtoDatabaseBase* which) const override;
    void onUserNotFoundOnMessage(const Message::Ptr& message) const override;
    bool rejectUid(const User::Ptr& user) const override;

   private:
    static std::string appendListName(const std::string& op,
                                      const User::Ptr from,
                                      const ProtoDatabaseBase* what);
};

struct DatabaseWrapper {
    // Save the changes to database file again
    void save(void) const;
    // 'Maybe' would return owner id stored in database
    UserId maybeGetOwnerId() const;

    DatabaseWrapper() = default;
    ~DatabaseWrapper() { save(); }

    std::shared_ptr<ProtoDatabaseBase> whitelist;
    std::shared_ptr<ProtoDatabaseBase> blacklist;
    Database protodb;

    std::filesystem::path& getDatabasePath() { return fname; }
    void load(const std::filesystem::path& file);

   private:
    bool warnNoLoaded(const char* func) const;
    std::filesystem::path fname;
    bool loaded = false;
    std::once_flag once;
};

struct DatabaseWrapperImpl : public DatabaseWrapper {
    // Load database
    // This method excludes blacklist/whitelist, syncthread
    virtual void load();
};

struct DatabaseWrapperBotImpl : public DatabaseWrapperImpl,
                                InitCall,
                                BotClassBase {
    DatabaseWrapperBotImpl(const Bot& bot) : BotClassBase(bot) {}
    NO_DEFAULT_CTOR(DatabaseWrapperBotImpl);

    // Load database (includes blacklist/whitelist, syncthread)
    void load() override;

    void doInitCall() override { load(); }
    const char* getInitCallName() const override {
        return "Load database, including white/black list";
    }
};

struct DatabaseWrapperImplObj : public DatabaseWrapperImpl,
                                InstanceClassBase<DatabaseWrapperImplObj> {};

struct DatabaseWrapperBotImplObj
    : public DatabaseWrapperBotImpl,
      InstanceClassBase<DatabaseWrapperBotImplObj> {
    NO_DEFAULT_CTOR(DatabaseWrapperBotImplObj);
    DatabaseWrapperBotImplObj(const Bot& bot) : DatabaseWrapperBotImpl(bot) {}
};

};  // namespace database
