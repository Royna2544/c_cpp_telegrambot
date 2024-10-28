#include "TgBotDatabaseImpl.hpp"

#include <Types.h>
#include <absl/log/log.h>

#include <StringToolsExt.hpp>
#include <database/DatabaseBase.hpp>
#include <filesystem>
#include <memory>
#include <optional>
#include <ostream>
#include <string_view>

#ifdef HAVE_PROTOBUF
#include <database/ProtobufDatabase.hpp>
#endif
#ifdef HAVE_SQLITE
#include <database/SQLiteDatabase.hpp>
#endif

TgBotDatabaseImpl::~TgBotDatabaseImpl() {
    // Unload the database if it was loaded
    if (loaded) {
        unloadDatabase();
    }
}

bool TgBotDatabaseImpl::setImpl(std::unique_ptr<DatabaseBase> impl) {
    if (_databaseImpl != nullptr) {
        LOG(WARNING) << "Implemention is already set.";
        return false;
    }
    _databaseImpl = std::move(impl);
    return true;
}

bool TgBotDatabaseImpl::setImpl(Providers provider) {
    if (!provider.chosenProvider) {
        LOG(WARNING) << "No provider chosen.";
        return false;
    }
    return setImpl(std::move(provider.chosenProvider));
}

bool TgBotDatabaseImpl::load(std::filesystem::path filepath) {
    if (loaded) {
        LOG(WARNING) << "Database is already loaded";
        return false;
    }
    if (!_databaseImpl) {
        LOG(WARNING) << "No implementation set.";
        return false;
    }

    loaded = _databaseImpl->load(filepath);
    return loaded;
}

bool TgBotDatabaseImpl::unloadDatabase() {
    if (loaded) {
        loaded = false;
        return _databaseImpl->unloadDatabase();
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

std::vector<TgBotDatabaseImpl::MediaInfo> TgBotDatabaseImpl::getAllMediaInfos()
    const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return {};
    }
    return _databaseImpl->getAllMediaInfos();
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

TgBotDatabaseImpl::Providers::Providers() {
#ifdef HAVE_SQLITE
    registerProvider("sqlite", std::make_unique<SQLiteDatabase>());
#endif
#ifdef HAVE_PROTOBUF
    registerProvider("protobuf", std::make_unique<ProtoDatabase>());
#endif
}

void TgBotDatabaseImpl::Providers::registerProvider(
    const std::string_view name, std::unique_ptr<DatabaseBase> provider) {
    // Check if the provider has already been registered
    if (_providers.contains(name)) {
        LOG(WARNING) << "Database provider with name " << SingleQuoted(name)
                     << " already registered.";
        return;
    }

    DLOG(INFO) << "TgBotDatabaseImpl::registerProvider: " << name;

    // Move the provider to the map and return
    _providers[name] = std::move(provider);
}

bool TgBotDatabaseImpl::Providers::chooseProvider(const std::string_view name) {
    if (_providers.contains(name)) {
        chosenProvider = std::move(_providers[name]);
        _providers.clear();
    } else {
        LOG(ERROR) << "Unsupported database backend: " << name;
        chosenProvider = nullptr;
    }
    return chosenProvider != nullptr;
}
