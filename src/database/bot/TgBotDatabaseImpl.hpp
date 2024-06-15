#pragma once

#include <database/ProtobufDatabase.hpp>
#include <database/SQLiteDatabase.hpp>
#include <variant>

#include "InstanceClassBase.hpp"

struct TgBotDatabaseImpl : InstanceClassBase<TgBotDatabaseImpl> {
    std::variant<ProtoDatabase, SQLiteDatabase> databaseBackend;
    bool loadDBFromConfig();

    ~TgBotDatabaseImpl();

    // Wrappers
    [[nodiscard]] bool isLoaded() const;
    [[nodiscard]] DatabaseBase::ListResult addUserToList(
        DatabaseBase::ListType type, UserId user) const;
    [[nodiscard]] DatabaseBase::ListResult removeUserFromList(
        DatabaseBase::ListType type, UserId user) const;
    [[nodiscard]] DatabaseBase::ListResult checkUserInList(
        DatabaseBase::ListType type, UserId user) const;
    [[nodiscard]] UserId getOwnerUserId() const;
    [[nodiscard]] std::optional<DatabaseBase::MediaInfo> queryMediaInfo(
        std::string str) const;
    [[nodiscard]] bool addMediaInfo(const DatabaseBase::MediaInfo& info) const;
    std::ostream &dump(std::ostream &ofs) const;

   private:
    template <typename T>
    constexpr static bool isKnownDatabase() {
        return std::is_same_v<T, ProtoDatabase> ||
               std::is_same_v<T, SQLiteDatabase>;
    }
    bool loaded = false;
};