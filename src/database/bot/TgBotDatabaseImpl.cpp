#include "TgBotDatabaseImpl.hpp"

#include <ConfigManager.h>

#include <database/ProtobufDatabase.hpp>
#include <database/SQLiteDatabase.hpp>
#include <filesystem>
#include <libos/libfs.hpp>
#include <memory>
#include <optional>
#include <ostream>
#include <system_error>

#include "Types.h"

bool TgBotDatabaseImpl::loadDBFromConfig() {
    const auto dbConf =
        ConfigManager::getVariable(ConfigManager::Configs::DATABASE_BACKEND);
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
            setImpl(std::make_unique<SQLiteDatabase>());
        } else if (backendStr == "protobuf") {
            setImpl(std::make_unique<ProtoDatabase>());
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
        loaded = loadDatabaseFromFile(parent / filenameStr);
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

bool TgBotDatabaseImpl::loadDatabaseFromFile(std::filesystem::path filepath) {
    bool isCreated = false; // i.e. Did it exist before?
    if (!FS::exists(filepath)) {
        DLOG(INFO) << "isCreated: true, creating new database file";
        isCreated = true;
    }
    bool _loaded = _databaseImpl->loadDatabaseFromFile(filepath);
    if (isCreated && _loaded) {
        _databaseImpl->initDatabase();
    }
    return _loaded;
}

bool TgBotDatabaseImpl::setImpl(std::unique_ptr<DatabaseBase> impl) {
    if (_databaseImpl != nullptr) {
        LOG(WARNING) << "Implemention is already set.";
        return false;
    }
    _databaseImpl = std::move(impl);
    return true;
}

bool TgBotDatabaseImpl::unloadDatabase() {
    if (loaded) {
        _databaseImpl->unloadDatabase();
    }
    return false;
}

bool TgBotDatabaseImpl::isLoaded() const { return loaded; }

DatabaseBase::ListResult TgBotDatabaseImpl::addUserToList(
    DatabaseBase::ListType type, UserId user) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return DatabaseBase::ListResult::BACKEND_ERROR;
    }
    return _databaseImpl->addUserToList(type, user);
}

DatabaseBase::ListResult TgBotDatabaseImpl::removeUserFromList(
    DatabaseBase::ListType type, UserId user) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return DatabaseBase::ListResult::BACKEND_ERROR;
    }
    return _databaseImpl->removeUserFromList(type, user);
}

DatabaseBase::ListResult TgBotDatabaseImpl::checkUserInList(
    DatabaseBase::ListType type, UserId user) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return DatabaseBase::ListResult::BACKEND_ERROR;
    }
    return _databaseImpl->checkUserInList(type, user);
}

std::optional<UserId> TgBotDatabaseImpl::getOwnerUserId() const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return std::nullopt;
    }
    return _databaseImpl->getOwnerUserId();
}

std::optional<DatabaseBase::MediaInfo> TgBotDatabaseImpl::queryMediaInfo(
    std::string str) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return std::nullopt;
    }
    return _databaseImpl->queryMediaInfo(str);
}

bool TgBotDatabaseImpl::addMediaInfo(
    const DatabaseBase::MediaInfo& info) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return false;
    }
    return _databaseImpl->addMediaInfo(info);
}

std::ostream& TgBotDatabaseImpl::dump(std::ostream& ofs) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return ofs;
    }
    return _databaseImpl->dump(ofs);
}

void TgBotDatabaseImpl::setOwnerUserId(const UserId user) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return;
    }
    _databaseImpl->setOwnerUserId(user);
}

bool TgBotDatabaseImpl::addChatInfo(const ChatId chatid,
                                    const std::string& name) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return false;
    }
    return _databaseImpl->addChatInfo(chatid, name);
}

std::optional<ChatId> TgBotDatabaseImpl::getChatId(
    const std::string& name) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return std::nullopt;
    }
    return _databaseImpl->getChatId(name);
}

DECLARE_CLASS_INST(TgBotDatabaseImpl);