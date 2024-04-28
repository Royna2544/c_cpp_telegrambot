#include <database/ProtobufDatabase.hpp>
#include <database/SQLiteDatabase.hpp>
#include <libos/libfs.hpp>

#include "BotClassBase.h"
#include "InstanceClassBase.hpp"
#include "initcalls/Initcall.hpp"

struct ProtoDatabaseBot : public ProtoDatabase,
                          InitCall,
                          BotClassBase,
                          InstanceClassBase<ProtoDatabaseBot> {
    explicit ProtoDatabaseBot(const TgBot::Bot& bot) : BotClassBase(bot) {}

    static std::filesystem::path getDatabaseDefaultPath() {
        return FS::getPathForType(FS::PathType::GIT_ROOT) / "tgbot.pb";
    }
    void doInitCall(void) override {
        loadDatabaseFromFile(getDatabaseDefaultPath());
    }
    const CStringLifetime getInitCallName() const override {
        return "Load database (Protobuf)";
    }
};

struct SQLiteDatabaseBot : public SQLiteDatabase,
                        InitCall,
                        BotClassBase,
                        InstanceClassBase<SQLiteDatabaseBot> {
    explicit SQLiteDatabaseBot(const TgBot::Bot& bot) : BotClassBase(bot) {}

    static std::filesystem::path getDatabaseDefaultPath() {
        return FS::getPathForType(FS::PathType::GIT_ROOT) / "tgbot.db";
    }
    void doInitCall(void) override {
        bool exists = std::filesystem::exists(getDatabaseDefaultPath());
        loadDatabaseFromFile(getDatabaseDefaultPath());
        if (!exists) {
            initDatabase();
        }
    }
    const CStringLifetime getInitCallName() const override {
        return "Load database (SQL)";
    }
};

using DefaultBotDatabase = SQLiteDatabaseBot;
using DefaultDatabase = SQLiteDatabase;