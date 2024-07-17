#pragma once

#include <TgBotDBImplExports.h>
#include <Types.h>

#include <InstanceClassBase.hpp>
#include <initcalls/Initcall.hpp>
#include <libos/OnTerminateRegistrar.hpp>
#include <memory>

#include "database/DatabaseBase.hpp"

struct TgBotDBImpl_API TgBotDatabaseImpl : InstanceClassBase<TgBotDatabaseImpl>,
                                           InitCall,
                                           DatabaseBase {
    ~TgBotDatabaseImpl() override = default;
    bool loadDBFromConfig();
    bool setImpl(std::unique_ptr<DatabaseBase> impl);
    bool unloadDatabase() override;

    // Wrappers
    [[nodiscard]] bool isLoaded() const;
    [[nodiscard]] DatabaseBase::ListResult addUserToList(
        DatabaseBase::ListType type, UserId user) const override;
    [[nodiscard]] DatabaseBase::ListResult removeUserFromList(
        DatabaseBase::ListType type, UserId user) const override;
    [[nodiscard]] DatabaseBase::ListResult checkUserInList(
        DatabaseBase::ListType type, UserId user) const override;
    [[nodiscard]] std::optional<UserId> getOwnerUserId() const override;
    [[nodiscard]] std::optional<DatabaseBase::MediaInfo> queryMediaInfo(
        std::string str) const override;
    [[nodiscard]] bool addMediaInfo(
        const DatabaseBase::MediaInfo &info) const override;
    std::ostream &dump(std::ostream &ofs) const override;
    void setOwnerUserId(UserId userid) const override;
    [[nodiscard]] bool addChatInfo(const ChatId chatid,
                                   const std::string &name) const override;
    [[nodiscard]] std::optional<ChatId> getChatId(
        const std::string &name) const override;

   private:
    bool loadDatabaseFromFile(std::filesystem::path filepath) override;

   public:
    const CStringLifetime getInitCallName() const override {
        return "Load database";
    }
    void doInitCall() override {
        loadDBFromConfig();
        OnTerminateRegistrar::getInstance()->registerCallback(
            [this]() { unloadDatabase(); });
    }

   private:
    std::unique_ptr<DatabaseBase> _databaseImpl;
    bool loaded = false;
};