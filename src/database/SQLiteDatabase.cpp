#include "SQLiteDatabase.hpp"

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <fstream>
#include <iomanip>
#include <libos/libfs.hpp>
#include <string_view>
#include <variant>

#include "CStringLifetime.h"
#include "Types.h"

SQLiteDatabase::ListResult SQLiteDatabase::addUserToList(InfoType type,
                                                         UserId user) const {
    ListResult res{};
    sqlite3_stmt* stmt = nullptr;

    res = checkUserInList(type, user);
    switch (res) {
        case ListResult::NOT_IN_LIST:
            break;
        case ListResult::ALREADY_IN_LIST:
            LOG(ERROR) << "Unexpected list result type "
                       << static_cast<int>(type) << " for user " << user;
            return res;
        case ListResult::OK:
            res = ListResult::ALREADY_IN_LIST;
            [[fallthrough]];
        case ListResult::ALREADY_IN_OTHER_LIST:
        case ListResult::BACKEND_ERROR:
            return res;
    }

    if (!loadAndPrepareSTMT(&stmt, "insertUser.sql")) {
        res = ListResult::BACKEND_ERROR;
        return res;
    }
    sqlite3_bind_int64(stmt, 1, user);
    sqlite3_bind_int(stmt, 2, static_cast<int>(type));
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        onSQLFail(__func__, "Inserting user");
    }
    sqlite3_finalize(stmt);
    return ListResult::OK;
}

SQLiteDatabase::ListResult SQLiteDatabase::addUserToList(ListType type,
                                                         UserId user) const {
    return addUserToList(toInfoType(type), user);
}

SQLiteDatabase::ListResult SQLiteDatabase::removeUserFromList(
    ListType type, UserId user) const {
    ListResult res{};
    sqlite3_stmt* stmt = nullptr;

    res = checkUserInList(type, user);
    switch (res) {
        case ListResult::OK:
            break;
        case ListResult::ALREADY_IN_LIST:
            LOG(ERROR) << "Unexpected list result type "
                       << static_cast<int>(type) << " for user " << user;
            [[fallthrough]];
        case ListResult::NOT_IN_LIST:
        case ListResult::ALREADY_IN_OTHER_LIST:
        case ListResult::BACKEND_ERROR:
            return res;
    }

    if (!loadAndPrepareSTMT(&stmt, "removeUser.sql")) {
        res = ListResult::BACKEND_ERROR;
        return res;
    }
    sqlite3_bind_int64(stmt, 1, user);
    sqlite3_bind_int(stmt, 2, static_cast<int>(toInfoType(type)));
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        onSQLFail(__func__, "Removing user");
    }
    sqlite3_finalize(stmt);
    return ListResult::OK;
}

void SQLiteDatabase::initDatabase() { loadAndExecuteSql("createDatabase.sql"); }

bool SQLiteDatabase::loadAndExecuteSql(const std::string_view filename) const {
    static auto sqlResPath = FS::getPathForType(FS::PathType::RESOURCES_SQL);

    auto sqlResFilePath = sqlResPath / filename.data();
    std::ifstream file(sqlResFilePath);
    std::string buffer;
    char* err_message = nullptr;

    if (!readSQLScriptFully(filename, buffer)) {
        return false;
    }
    LOG(INFO) << "SQL file " << sqlResFilePath << " executing...";
    if (sqlite3_exec(db, buffer.c_str(), nullptr, nullptr, &err_message) !=
        SQLITE_OK) {
        onSQLFail(__func__, "Executing SQL script");
        sqlite3_free(err_message);
        return false;
    } else {
        DLOG(INFO) << "Executed SQL script successfully.";
    }
    return true;
}

[[nodiscard]] DatabaseBase::ListResult SQLiteDatabase::checkUserInList(
    ListType type, UserId user) const {
    return checkUserInList(toInfoType(type), user);
}

[[nodiscard]] DatabaseBase::ListResult SQLiteDatabase::checkUserInList(
    InfoType type, UserId user) const {
    ListResult result = ListResult::BACKEND_ERROR;
    sqlite3_stmt* stmt = nullptr;
    int rc = 0;

    if (!loadAndPrepareSTMT(&stmt, "findUser.sql")) {
        return result;
    }
    sqlite3_bind_int64(stmt, 1, user);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        auto info = static_cast<InfoType>(sqlite3_column_int(stmt, 0));
        auto reqinfo = type;
        if (info == reqinfo) {
            result = ListResult::OK;
        } else {
            // Not in this list, but other
            result = ListResult::ALREADY_IN_OTHER_LIST;
        }
    } else if (rc != SQLITE_DONE) {
        onSQLFail(__func__, "Check user in list");
        result = ListResult::BACKEND_ERROR;
    } else {
        // Not in this list, just doesn't exist
        result = ListResult::NOT_IN_LIST;
    }
    sqlite3_finalize(stmt);
    return result;
}

bool SQLiteDatabase::loadDatabaseFromFile(std::filesystem::path filepath) {
    int ret = 0;

    if (db != nullptr) {
        return false;
    }
    ret = sqlite3_open(filepath.string().c_str(), &db);
    if (ret != SQLITE_OK) {
        LOG(ERROR) << "Could not open database: " << sqlite3_errmsg(db);
        return false;
    }
    LOG(INFO) << "Loaded SQLite database: " << filepath;
    return true;
}

bool SQLiteDatabase::unloadDatabase() {
    if (db != nullptr) {
        if (sqlite3_close(db) == SQLITE_OK) {
            db = nullptr;
            return true;
        } else {
            LOG(ERROR) << "Could not close database: " << sqlite3_errmsg(db);
            return false;
        }
    }
    return false;
}

bool SQLiteDatabase::loadAndPrepareSTMT(sqlite3_stmt** stmt,
                                        const std::string_view filename) const {
    std::string sqlScriptData;

    if (!readSQLScriptFully(filename, sqlScriptData)) {
        return false;
    }
    int rc = sqlite3_prepare_v2(db, sqlScriptData.c_str(), -1, stmt, nullptr);
    if (rc != SQLITE_OK) {
        onSQLFail(__func__, "PrepareV2");
        return false;
    }
    return true;
}

bool SQLiteDatabase::readSQLScriptFully(const std::string_view filename,
                                        std::string& out_data) {
    static auto sqlResPath = FS::getPathForType(FS::PathType::RESOURCES_SQL);
    std::ifstream sqlFile(sqlResPath / filename.data());
    if (!sqlFile.is_open()) {
        LOG(ERROR) << "Could not open SQL script file: " << filename;
        return false;
    }
    out_data = std::string((std::istreambuf_iterator<char>(sqlFile)),
                           std::istreambuf_iterator<char>());
    return true;
}

std::optional<SQLiteDatabase::MediaInfo> SQLiteDatabase::queryMediaInfo(
    std::string str) const {
    sqlite3_stmt* stmt = nullptr;
    ListResult ret{};
    MediaInfo info{};
    int rc = 0;

    if (!loadAndPrepareSTMT(&stmt, "findMediaInfo.sql")) {
        return std::nullopt;
    }
    sqlite3_bind_text(stmt, 1, str.c_str(), -1, SQLITE_STATIC);
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        const auto* mediaId =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const auto* mediaUniqueId =
            reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if ((mediaId != nullptr) && (mediaUniqueId != nullptr)) {
            info.mediaId = mediaId;
            info.mediaUniqueId = mediaUniqueId;
        } else {
            rc = SQLITE_ERROR;
        }
    }
    if (rc != SQLITE_ROW) {
        onSQLFail(__func__, "Querying media ids");
        sqlite3_finalize(stmt);
        return std::nullopt;
    }
    sqlite3_finalize(stmt);
    return info;
}

bool SQLiteDatabase::addMediaInfo(const MediaInfo& info) const {
    struct UpdateInfo {
        enum class Op {
            INSERT,  // This name does not exist in namemap: I should insert it
            USE_EXISTING,  // This name exists in namemap
        } update{};
        // id of namemap for USE_EXISTING, string data for INSERT
        std::variant<int, std::string> data;
    };
    sqlite3_stmt* stmt = nullptr;
    int count = 0;
    int mediaIdIndex = 0;
    std::vector<UpdateInfo> updates(info.names.size());

    if (info.names.size() == 0) {
        LOG(ERROR) << "Zero-length names specified";
        return false;  // No names to insert, so no need to run the query
    }

    // Determine stuff to insert, and the ones that already exist
    for (const auto& name : info.names) {
        int ret = 0;

        if (!loadAndPrepareSTMT(&stmt, "findMediaName.sql")) {
            return false;
        }
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_STATIC);
        ret = sqlite3_step(stmt);
        if (ret == SQLITE_ROW) {
            updates[count].update = UpdateInfo::Op::USE_EXISTING;
            updates[count].data = sqlite3_column_int(stmt, 0);
        } else if (ret == SQLITE_DONE) {
            updates[count].update = UpdateInfo::Op::INSERT;
            updates[count].data = name;
        } else {
            onSQLFail(__func__, "Finding media name");
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_finalize(stmt);
        count++;
    }

    // Insert the names into the database
    for (auto& info : updates) {
        switch (info.update) {
            case UpdateInfo::Op::INSERT: {
                CStringLifetime name = std::get<std::string>(info.data);

                // Insert into database
                if (!loadAndPrepareSTMT(&stmt, "insertMediaName.sql")) {
                    return false;
                }
                sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
                if (sqlite3_step(stmt) != SQLITE_DONE) {
                    onSQLFail(__func__, "Inserting medianame");
                    sqlite3_finalize(stmt);
                    return false;
                }
                sqlite3_finalize(stmt);

                // Get the index again
                if (!loadAndPrepareSTMT(&stmt, "findMediaName.sql")) {
                    return false;
                }
                sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
                if (sqlite3_step(stmt) != SQLITE_ROW) {
                    onSQLFail(__func__, "Getting index of inserted medianame");
                    sqlite3_finalize(stmt);
                    return false;
                }
                info.data = sqlite3_column_int(stmt, 0);
                sqlite3_finalize(stmt);
                break;
            }

            case UpdateInfo::Op::USE_EXISTING:
                break;
        }
    }

    // Insert the media info into the database
    if (!loadAndPrepareSTMT(&stmt, "insertMediaId.sql")) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, info.mediaUniqueId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, info.mediaId.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        onSQLFail(__func__, "Inserting tg media id");
        sqlite3_finalize(stmt);
        return false;
    }

    // Get the inserted media index
    if (!loadAndPrepareSTMT(&stmt, "findMediaId.sql")) {
        return false;
    }
    sqlite3_bind_text(stmt, 1, info.mediaUniqueId.c_str(), -1, SQLITE_STATIC);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        mediaIdIndex = sqlite3_column_int(stmt, 0);
    } else {
        onSQLFail(__func__, "Getting index of inserted tg media id");
        sqlite3_finalize(stmt);
        return false;
    }
    sqlite3_finalize(stmt);

    // Insert the actual map into the database
    for (const auto& info : updates) {
        int data = std::get<int>(info.data);

        if (!loadAndPrepareSTMT(&stmt, "insertMediaMap.sql")) {
            return false;
        }
        sqlite3_bind_int(stmt, 1, mediaIdIndex);
        sqlite3_bind_int(stmt, 2, data);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            onSQLFail(__func__, "Inserting media info");
            sqlite3_finalize(stmt);
            return false;
        }
    }
    sqlite3_finalize(stmt);
    return true;
}

UserId SQLiteDatabase::getOwnerUserId() const {
    UserId id = kInvalidUserId;
    sqlite3_stmt* stmt = nullptr;
    int rc = 0;

    if (!loadAndPrepareSTMT(&stmt, "findOwner.sql")) {
        return kInvalidUserId;
    }
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        id = sqlite3_column_int64(stmt, 0);
    } else if (rc != SQLITE_DONE) {
        onSQLFail(__func__, "Getting column to owner id");
    }
    sqlite3_finalize(stmt);
    return id;
}

SQLiteDatabase::InfoType SQLiteDatabase::toInfoType(ListType type) {
    switch (type) {
        case ListType::BLACKLIST:
            return InfoType::BLACKLIST;
        case ListType::WHITELIST:
            return InfoType::WHITELIST;
    }
    CHECK(false) << "Unreachable";
}

std::ostream& SQLiteDatabase::dump(std::ostream& os) const {
    sqlite3_stmt* stmt = nullptr;
    int rc = 0;
    std::stringstream ss;

    if (db == nullptr) {
        os << "Database not loaded!";
        return os;
    }

    ss << "====================== Dump of database ======================"
       << std::endl;

    // Because of the race condition with logging, use stringstream and output
    // later.
    ss << "Owner Id: " << std::quoted(std::to_string(getOwnerUserId()))
       << std::endl;

    if (loadAndPrepareSTMT(&stmt, "dumpDatabase.sql")) {
        rc = sqlite3_step(stmt);
        while (rc == SQLITE_ROW) {
            int type = 0;
            ss << "UserId: " << sqlite3_column_int64(stmt, 0);
            type = sqlite3_column_int(stmt, 1);
            if (type > static_cast<int>(InfoType::WHITELIST) ||
                type < static_cast<int>(InfoType::OWNER)) {
                ss << " type: Invalid(" << type << ")" << std::endl;
                continue;
            }
            auto info = static_cast<InfoType>(sqlite3_column_int(stmt, 1));
            ss << " type: ";
            switch (info) {
                case InfoType::BLACKLIST:
                    ss << "BLACKLIST";
                    break;
                case InfoType::WHITELIST:
                    ss << "WHITELIST";
                    break;
                case InfoType::OWNER:
                    ss << "OWNER";
                    break;
            };
            ss << std::endl;
            rc = sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);
        ss << std::endl;
    } else {
        ss << "!!! Failed to dump usermap database" << std::endl;
    }

    if (loadAndPrepareSTMT(&stmt, "dumpDatabaseMedia.sql")) {
        rc = sqlite3_step(stmt);
        while (rc == SQLITE_ROW) {
            ss << "MediaId: " << sqlite3_column_text(stmt, 0) << std::endl;
            ss << "MediaUniqueId: " << sqlite3_column_text(stmt, 1)
               << std::endl;
            ss << "MediaName: " << sqlite3_column_text(stmt, 2) << std::endl;
            rc = sqlite3_step(stmt);
            ss << std::endl;
        }
        sqlite3_finalize(stmt);
    } else {
        ss << "!!! Failed to dump media database" << std::endl;
    }
    os << ss.str();
    return os;
}

void SQLiteDatabase::onSQLFail(const std::string_view funcname,
                               const std::string_view what) const {
    LOG(ERROR) << "!!! " << funcname
               << " failed: SQL error: " << sqlite3_errmsg(db) << " while "
               << std::quoted(what);
}

void SQLiteDatabase::setOwnerUserId(UserId userId) const {
    switch (addUserToList(InfoType::OWNER, userId)) {
        case DatabaseBase::ListResult::OK:
            LOG(INFO) << "Owner set to " << userId;
            break;
        case DatabaseBase::ListResult::ALREADY_IN_OTHER_LIST:
        case DatabaseBase::ListResult::BACKEND_ERROR:
            LOG(ERROR) << "Failed to set owner to " << userId;
            break;
        case DatabaseBase::ListResult::ALREADY_IN_LIST:
            DLOG(INFO) << "Owner already set to " << userId;
            break;
        case DatabaseBase::ListResult::NOT_IN_LIST:
            // Not possible
            break;
    }
}