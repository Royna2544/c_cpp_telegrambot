#pragma once

#include <TgBotDBImplExports.h>
#include <Types.h>
#include <trivial_helpers/_class_helper_macros.h>

#include <ConfigManager.hpp>
#include <database/DatabaseBase.hpp>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string_view>

/**
 * @brief TgBotDatabaseImpl is a class that implements the DatabaseBase
 * interface for managing bot data. It provides functionality for interacting
 * with different database providers.
 */
struct TgBotDBImpl_API TgBotDatabaseImpl : DatabaseBase {
    /**
     * @brief Providers is a nested struct that manages different database
     * providers.
     */
    struct TgBotDBImpl_API Providers {
        Providers();

        /**
         * @brief Registers a new database provider with the given name.
         * @param name The name of the provider.
         * @param provider A unique pointer to the database provider instance.
         */
        void registerProvider(const std::string_view name,
                              std::unique_ptr<DatabaseBase> provider);

        /**
         * @brief Chooses the database provider with the given name.
         * @param name The name of the provider.
         * @return True if the provider is successfully chosen, false otherwise.
         */
        bool chooseProvider(const std::string_view name);

        /**
         * @brief Chooses any available database provider.
         * @return True if the provider is successfully chosen, false otherwise.
         */
        bool chooseAnyProvider();

        friend struct TgBotDatabaseImpl;

       private:
        // Not owning the string as it will always be a literal
        std::map<std::string_view, std::unique_ptr<DatabaseBase>> _providers;
        std::unique_ptr<DatabaseBase> chosenProvider;
    };

    APPLE_INJECT(TgBotDatabaseImpl()) = default;
    ~TgBotDatabaseImpl() override;

    NO_COPY_CTOR(TgBotDatabaseImpl);

    // Unload the database
    bool unload() override;

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
    [[nodiscard]] AddResult addMediaInfo(
        const DatabaseBase::MediaInfo &info) const override;
    [[nodiscard]] std::vector<MediaInfo> getAllMediaInfos() const override;
    std::ostream &dump(std::ostream &ofs) const override;
    void setOwnerUserId(UserId userid) const override;
    [[nodiscard]] AddResult addChatInfo(
        const ChatId chatid, const std::string_view name) const override;
    [[nodiscard]] std::optional<ChatId> getChatId(
        const std::string_view name) const override;

    // Load database from file
    bool load(std::filesystem::path filepath) override;

    bool setImpl(Providers providers);

   private:
    // Takes a std::unique_ptr containing the implementation
    bool setImpl(std::unique_ptr<DatabaseBase> impl);

    std::unique_ptr<DatabaseBase> _databaseImpl;
    bool loaded = false;
};

extern bool TgBotDatabaseImpl_load(ConfigManager *configmgr,
                                   TgBotDatabaseImpl *dbimpl);