#pragma once

#include <TgBotDBImplExports.h>
#include <Types.h>
#include <trivial_helpers/_class_helper_macros.h>

#include <InitTask.hpp>
#include <InstanceClassBase.hpp>
#include <database/DatabaseBase.hpp>
#include <map>
#include <memory>

struct TgBotDBImpl_API TgBotDatabaseImpl : InstanceClassBase<TgBotDatabaseImpl>,
                                           DatabaseBase {
    struct TgBotDBImpl_API Providers {
        Providers();

        void registerProvider(const std::string_view name,
                              std::unique_ptr<DatabaseBase> provider);
        bool chooseProvider(const std::string_view name);

        friend struct TgBotDatabaseImpl;

       private:
        // Not owning the string as it will always be a literal
        std::map<std::string_view, std::unique_ptr<DatabaseBase>> _providers;
        std::unique_ptr<DatabaseBase> chosenProvider;
    };

    TgBotDatabaseImpl() = default;
    ~TgBotDatabaseImpl() override;

    NO_COPY_CTOR(TgBotDatabaseImpl);

    // Unload the database
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
    [[nodiscard]] std::vector<MediaInfo> getAllMediaInfos() const override;
    std::ostream &dump(std::ostream &ofs) const override;
    void setOwnerUserId(UserId userid) const override;
    [[nodiscard]] bool addChatInfo(const ChatId chatid,
                                   const std::string &name) const override;
    [[nodiscard]] std::optional<ChatId> getChatId(
        const std::string &name) const override;

    // Load database from file
    bool load(std::filesystem::path filepath) override;

    bool setImpl(Providers providers);
   private:
    // Takes a std::unique_ptr containing the implementation
    bool setImpl(std::unique_ptr<DatabaseBase> impl);

    std::unique_ptr<DatabaseBase> _databaseImpl;
    bool loaded = false;
};
