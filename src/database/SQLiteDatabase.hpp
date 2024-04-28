#pragma once

#include <sqlite3.h>

#include <string>
#include <string_view>

#include "DatabaseBase.hpp"

struct SQLiteDatabase : DatabaseBase {
    enum class InfoType {
        OWNER = 0,
        BLACKLIST = 1,
        WHITELIST = 2,
    };
    [[nodiscard]] ListResult addUserToList(ListType type, UserId user) override;
    [[nodiscard]] ListResult removeUserFromList(ListType type,
                                                UserId user) override;
    [[nodiscard]] ListResult checkUserInList(ListType type,
                                             UserId user) const override;
    bool loadDatabaseFromFile(std::filesystem::path filepath) override;
    bool unloadDatabase() override;
    [[nodiscard]] UserId getOwnerUserId() const override;
    void initDatabase() override;
    std::optional<MediaInfo> queryMediaInfo(std::string str) const override;
    bool addMediaInfo(const MediaInfo &info) const override;
    friend std::ostream &operator<<(std::ostream &os, SQLiteDatabase sqDB);

   private:
    bool loadAndExecuteSql(const std::string_view filename) const;
    bool loadAndPrepareSTMT(sqlite3_stmt **stmt,
                            const std::string_view filename) const;
    static bool readSQLScriptFully(const std::string_view filename,
                                   std::string &out_data);
    void onSQLFail(const std::string_view funcname,
                   const std::string_view what) const;

    [[nodiscard]] ListResult checkUserInList(InfoType type, UserId user) const;
    static InfoType toInfoType(ListType type);
    sqlite3 *db = nullptr;
};