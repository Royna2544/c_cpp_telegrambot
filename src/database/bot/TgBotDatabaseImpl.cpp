#include "TgBotDatabaseImpl.hpp"

#include <ConfigManager.h>

#include <filesystem>
#include <libos/libfs.hpp>
#include <ostream>
#include <system_error>

#include "Types.h"
#include "database/SQLiteDatabase.hpp"

bool TgBotDatabaseImpl::loadDBFromConfig() {
    const auto dbConf =
        ConfigManager::getVariable(ConfigManager::Configs::DATABASE_BACKEND);
    bool isCreated = false;
    std::error_code ec;

    if (dbConf) {
        const std::string& config = dbConf.value();
        DLOG(INFO) << "Database configuration string: "
                   << std::quoted(dbConf.value());
        const auto speratorIdx = config.find(':');

        if (speratorIdx == std::string::npos) {
            LOG(ERROR) << "Invalid database configuration";
            return false;
        }
        // Expected format: <backend>:filename relative to git root
        const auto backendStr = config.substr(0, speratorIdx);
        const auto filenameStr = config.substr(speratorIdx + 1);
        DLOG(INFO) << "Database backend: " << backendStr;
        DLOG(INFO) << "Database filename: " << filenameStr;

        if (backendStr == "sqlite") {
            databaseBackend = SQLiteDatabase();
        } else if (backendStr == "protobuf") {
            databaseBackend = ProtoDatabase();
        } else {
            LOG(ERROR) << "Invalid database backend: " << backendStr;
            return false;
        }

        std::filesystem::path parent;
        if (std::filesystem::path(filenameStr).is_relative()) {
            parent = std::filesystem::current_path(ec);
            if (ec) {
                LOG(ERROR) << "Failed to get current path: " << ec.message();
                return false;
            }
        }
        if (!FS::exists(parent / filenameStr)) {
            DLOG(INFO) << "isCreated: true, creating new database file";
            isCreated = true;
        }
        std::visit(
            [filenameStr, this, isCreated, parent](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (isKnownDatabase<T>()) {
                    loaded = arg.loadDatabaseFromFile(parent / filenameStr);
                    if (isCreated && loaded) {
                        arg.initDatabase();
                    }
                }
            },
            databaseBackend);
        if (!loaded) {
            LOG(ERROR) << "Failed to load database from file";
        } else {
            DLOG(INFO) << "Database loaded";
        }
    }
    if (!loaded) {
        LOG(ERROR) << "Failed to load database, the bot will not be able to "
                      "save changes.";
    }
    return loaded;
}

TgBotDatabaseImpl::~TgBotDatabaseImpl() {
    if (loaded) {
        std::visit(
            [this](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (isKnownDatabase<T>()) {
                    arg.unloadDatabase();
                }
            },
            databaseBackend);
    }
}

bool TgBotDatabaseImpl::isLoaded() const { return loaded; }

DatabaseBase::ListResult TgBotDatabaseImpl::addUserToList(
    DatabaseBase::ListType type, UserId user) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return DatabaseBase::ListResult::BACKEND_ERROR;
    }
    return std::visit(
        [this, type, user](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (isKnownDatabase<T>()) {
                return arg.addUserToList(type, user);
            }
        },
        databaseBackend);
}

DatabaseBase::ListResult TgBotDatabaseImpl::removeUserFromList(
    DatabaseBase::ListType type, UserId user) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return DatabaseBase::ListResult::BACKEND_ERROR;
    }
    return std::visit(
        [this, type, user](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (isKnownDatabase<T>()) {
                return arg.removeUserFromList(type, user);
            }
        },
        databaseBackend);
}

DatabaseBase::ListResult TgBotDatabaseImpl::checkUserInList(
    DatabaseBase::ListType type, UserId user) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return DatabaseBase::ListResult::BACKEND_ERROR;
    }
    return std::visit(
        [this, type, user](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (isKnownDatabase<T>()) {
                return arg.checkUserInList(type, user);
            }
        },
        databaseBackend);
}

UserId TgBotDatabaseImpl::getOwnerUserId() const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return DatabaseBase::kInvalidUserId;
    }
    return std::visit(
        [this](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (isKnownDatabase<T>()) {
                return arg.getOwnerUserId();
            }
        },
        databaseBackend);
}

std::optional<DatabaseBase::MediaInfo> TgBotDatabaseImpl::queryMediaInfo(
    std::string str) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return std::nullopt;
    }
    return std::visit(
        [this, str](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (isKnownDatabase<T>()) {
                return arg.queryMediaInfo(str);
            }
        },
        databaseBackend);
}

bool TgBotDatabaseImpl::addMediaInfo(
    const DatabaseBase::MediaInfo& info) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return false;
    }
    return std::visit(
        [this, info](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (isKnownDatabase<T>()) {
                return arg.addMediaInfo(info);
            }
        },
        databaseBackend);
}

std::ostream& TgBotDatabaseImpl::dump(std::ostream& ofs) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return ofs;
    }
    std::visit(
        [this, &ofs](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (isKnownDatabase<T>()) {
                arg.dump(ofs);
            }
        },
        databaseBackend);
    return ofs;
}

void TgBotDatabaseImpl::setOwnerUserId(const UserId user) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return;
    }
    std::visit(
        [this, user](auto&& arg) {
            using T = std::decay_t<decltype(arg)>;
            if constexpr (isKnownDatabase<T>()) {
                arg.setOwnerUserId(user);
            }
        },
        databaseBackend);
}

DECLARE_CLASS_INST(TgBotDatabaseImpl);