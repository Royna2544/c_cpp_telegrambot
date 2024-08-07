#include "TgBotDatabaseImpl.hpp"

#include <ConfigManager.h>

#ifdef HAVE_PROTOBUF
#include <database/ProtobufDatabase.hpp>
#endif
#ifdef HAVE_SQLITE
#include <database/SQLiteDatabase.hpp>
#endif
#include <filesystem>
#include <libos/libfs.hpp>
#include <memory>
#include <optional>
#include <ostream>
#include <system_error>

#include "Types.h"

bool TgBotDatabaseImpl::setImpl(std::unique_ptr<DatabaseBase> impl) {
    if (_databaseImplRaw != nullptr) {
        LOG(WARNING) << "Implemention is already set.";
        return false;
    }
    _databaseImpl = std::move(impl);
    _databaseImplRaw = _databaseImpl.get();
    return true;
}

bool TgBotDatabaseImpl::setImpl(DatabaseBase* impl) {
    if (_databaseImplRaw != nullptr) {
        LOG(WARNING) << "Implemention is already set.";
        return false;
    }
    _databaseImpl = nullptr;
    _databaseImplRaw = impl;
    return true;
}

bool TgBotDatabaseImpl::loadDatabaseFromFile(std::filesystem::path filepath) {
    bool isCreated = false;  // i.e. Did it exist before?

    if (loaded) {
        LOG(WARNING) << "Database is already loaded";
        return false;
    }
    if (!FS::exists(filepath)) {
        DLOG(INFO) << "isCreated: true, creating new database file";
        isCreated = true;
    }
    loaded = _databaseImplRaw->loadDatabaseFromFile(filepath);
    if (isCreated && loaded) {
        _databaseImplRaw->initDatabase();
    }
    return loaded;
}

bool TgBotDatabaseImpl::unloadDatabase() {
    if (loaded) {
        loaded = false;
        return _databaseImplRaw->unloadDatabase();
    } else {
        LOG(WARNING) << "No database to unload.";
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
    return _databaseImplRaw->addUserToList(type, user);
}

DatabaseBase::ListResult TgBotDatabaseImpl::removeUserFromList(
    DatabaseBase::ListType type, UserId user) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return DatabaseBase::ListResult::BACKEND_ERROR;
    }
    return _databaseImplRaw->removeUserFromList(type, user);
}

DatabaseBase::ListResult TgBotDatabaseImpl::checkUserInList(
    DatabaseBase::ListType type, UserId user) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return DatabaseBase::ListResult::BACKEND_ERROR;
    }
    return _databaseImplRaw->checkUserInList(type, user);
}

std::optional<UserId> TgBotDatabaseImpl::getOwnerUserId() const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return std::nullopt;
    }
    return _databaseImplRaw->getOwnerUserId();
}

std::optional<DatabaseBase::MediaInfo> TgBotDatabaseImpl::queryMediaInfo(
    std::string str) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return std::nullopt;
    }
    return _databaseImplRaw->queryMediaInfo(str);
}

bool TgBotDatabaseImpl::addMediaInfo(
    const DatabaseBase::MediaInfo& info) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return false;
    }
    return _databaseImplRaw->addMediaInfo(info);
}

std::ostream& TgBotDatabaseImpl::dump(std::ostream& ofs) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return ofs;
    }
    return _databaseImplRaw->dump(ofs);
}

void TgBotDatabaseImpl::setOwnerUserId(const UserId user) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return;
    }
    _databaseImplRaw->setOwnerUserId(user);
}

bool TgBotDatabaseImpl::addChatInfo(const ChatId chatid,
                                    const std::string& name) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return false;
    }
    return _databaseImplRaw->addChatInfo(chatid, name);
}

std::optional<ChatId> TgBotDatabaseImpl::getChatId(
    const std::string& name) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return std::nullopt;
    }
    return _databaseImplRaw->getChatId(name);
}

void TgBotDatabaseImpl::doInitCall() {
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
            return;
        }
        // Expected format: <backend>:filename relative to git root (Could be
        // absolute)
        const auto backendStr = config.substr(0, speratorIdx);
        const auto filenameStr = config.substr(speratorIdx + 1);
        DLOG(INFO) << "Database backend: " << backendStr;
        DLOG(INFO) << "Database filename: " << filenameStr;

        if (backendStr == "sqlite") {
#ifdef HAVE_SQLITE
            setImpl(std::make_unique<SQLiteDatabase>());
#else
            LOG(ERROR) << "SQLite support is not available in this build";
            goto fail;
#endif
        } else if (backendStr == "protobuf") {
#ifdef HAVE_PROTOBUF
            setImpl(std::make_unique<ProtoDatabase>());
#else
            LOG(ERROR) << "Proto support is not available in this build";
            goto fail;
#endif
        } else {
            LOG(ERROR) << "Invalid database backend: " << backendStr;
            goto fail;
        }

        std::filesystem::path parent;
        if (std::filesystem::path(filenameStr).is_relative()) {
            parent = std::filesystem::current_path(ec);
            if (ec) {
                LOG(ERROR) << "Failed to get current path: " << ec.message();
                return;
            }
        }
        loaded = loadDatabaseFromFile(parent / filenameStr);
        if (!loaded) {
            LOG(ERROR) << "Failed to load database from file";
        } else {
            DLOG(INFO) << "Database loaded";
        }
    }
fail:
    if (!loaded) {
        LOG(ERROR) << "Failed to load database, the bot will not be able to "
                      "save changes.";
    } else {
        OnTerminateRegistrar::getInstance()->registerCallback(
            [this]() { unloadDatabase(); });
    }
}
DECLARE_CLASS_INST(TgBotDatabaseImpl);