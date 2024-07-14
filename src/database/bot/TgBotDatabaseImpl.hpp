#pragma once

#include <TgBotDBImplExports.h>
#include <Types.h>

#include <InstanceClassBase.hpp>
#include <database/ProtobufDatabase.hpp>
#include <database/SQLiteDatabase.hpp>
#include <initcalls/Initcall.hpp>
#include <libos/OnTerminateRegistrar.hpp>
#include <variant>

struct TgBotDBImpl_API TgBotDatabaseImpl : InstanceClassBase<TgBotDatabaseImpl>,
                                           InitCall {
    virtual ~TgBotDatabaseImpl() = default;
    std::variant<ProtoDatabase, SQLiteDatabase> databaseBackend;
    bool loadDBFromConfig();
    void unloadDatabase();

    // Wrappers
    [[nodiscard]] bool isLoaded() const;
    [[nodiscard]] DatabaseBase::ListResult addUserToList(
        DatabaseBase::ListType type, UserId user) const;
    [[nodiscard]] DatabaseBase::ListResult removeUserFromList(
        DatabaseBase::ListType type, UserId user) const;
    [[nodiscard]] DatabaseBase::ListResult checkUserInList(
        DatabaseBase::ListType type, UserId user) const;
    [[nodiscard]] std::optional<UserId> getOwnerUserId() const;
    [[nodiscard]] std::optional<DatabaseBase::MediaInfo> queryMediaInfo(
        std::string str) const;
    [[nodiscard]] bool addMediaInfo(const DatabaseBase::MediaInfo &info) const;
    std::ostream &dump(std::ostream &ofs) const;
    void setOwnerUserId(UserId userid) const;
    [[nodiscard]] bool addChatInfo(const ChatId chatid,
                                   const std::string &name) const;
    [[nodiscard]] std::optional<ChatId> getChatId(
        const std::string &name) const;

    const CStringLifetime getInitCallName() const override {
        return "Load database";
    }
    void doInitCall() override {
        loadDBFromConfig();
        OnTerminateRegistrar::getInstance()->registerCallback(
            [this]() { unloadDatabase(); });
    }

   private:
    template <typename T>
    constexpr static bool isKnownDatabase() {
        return std::is_same_v<T, ProtoDatabase> ||
               std::is_same_v<T, SQLiteDatabase>;
    }
    bool loaded = false;
};