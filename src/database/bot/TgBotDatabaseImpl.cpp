#include "TgBotDatabaseImpl.hpp"

#include <Types.h>
#include <absl/log/log.h>
#include <fmt/format.h>

#include <database/DatabaseBase.hpp>
#include <filesystem>
#include <memory>
#include <optional>
#include <ostream>
#include <string_view>

#ifdef DATABASE_HAVE_PROTOBUF
#include <database/ProtobufDatabase.hpp>
#endif
#ifdef DATABASE_HAVE_SQLITE
#include <database/SQLiteDatabase.hpp>
#endif

TgBotDatabaseImpl::~TgBotDatabaseImpl() {
    // Unload the database if it was loaded
    if (loaded) {
        unload();
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

    try {
        loaded = _databaseImpl->load(filepath);
    } catch (const std::exception& ex) {
        LOG(ERROR) << "Failed to load database: " << ex.what();
        return false;
    }
    return loaded;
}

bool TgBotDatabaseImpl::unload() {
    if (loaded) {
        loaded = false;
        return _databaseImpl->unload();
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
    try {
        return _databaseImpl->addUserToList(type, user);
    } catch (const exception& ex) {
        LOG(ERROR) << "Failed to add user to list: " << ex.what();
        return DatabaseBase::ListResult::BACKEND_ERROR;
    }
}

DatabaseBase::ListResult TgBotDatabaseImpl::removeUserFromList(
    DatabaseBase::ListType type, UserId user) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return DatabaseBase::ListResult::BACKEND_ERROR;
    }
    try {
        return _databaseImpl->removeUserFromList(type, user);
    } catch (const exception& e) {
        LOG(ERROR) << "Failed to remove user from list: " << e.what();
        return DatabaseBase::ListResult::BACKEND_ERROR;
    }
}

DatabaseBase::ListResult TgBotDatabaseImpl::checkUserInList(
    DatabaseBase::ListType type, UserId user) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return DatabaseBase::ListResult::BACKEND_ERROR;
    }
    try {
        return _databaseImpl->checkUserInList(type, user);
    } catch (const exception& ex) {
        LOG(ERROR) << "Failed to check user in list: " << ex.what();
        return DatabaseBase::ListResult::BACKEND_ERROR;
    }
}

std::optional<UserId> TgBotDatabaseImpl::getOwnerUserId() const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return std::nullopt;
    }
    try {
        return _databaseImpl->getOwnerUserId();
    } catch (const exception& ex) {
        LOG(ERROR) << "Failed to get owner user ID: " << ex.what();
        return std::nullopt;
    }
}

std::optional<DatabaseBase::MediaInfo> TgBotDatabaseImpl::queryMediaInfo(
    std::string str) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return std::nullopt;
    }
    try {
        return _databaseImpl->queryMediaInfo(str);
    } catch (const exception& ex) {
        LOG(ERROR) << "Failed to query media info: " << ex.what();
        return std::nullopt;
    }
}

TgBotDatabaseImpl::AddResult TgBotDatabaseImpl::addMediaInfo(
    const DatabaseBase::MediaInfo& info) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return AddResult::BACKEND_ERROR;
    }
    try {
        return _databaseImpl->addMediaInfo(info);
    } catch (const exception& ex) {
        LOG(ERROR) << "Failed to add media info: " << ex.what();
        return AddResult::BACKEND_ERROR;
    }
}

std::vector<TgBotDatabaseImpl::MediaInfo> TgBotDatabaseImpl::getAllMediaInfos()
    const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return {};
    }
    try {
        return _databaseImpl->getAllMediaInfos();
    } catch (const exception &ex) {
        LOG(ERROR) << "Failed to get all media infos: " << ex.what();
        return {};
    }
}

std::ostream& TgBotDatabaseImpl::dump(std::ostream& ofs) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return ofs;
    }
    try {
        return _databaseImpl->dump(ofs);
    } catch (const std::exception& ex) {
        LOG(ERROR) << "Failed to dump database: " << ex.what();
        return ofs;
    }
}

void TgBotDatabaseImpl::setOwnerUserId(const UserId user) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return;
    }
    try {
        _databaseImpl->setOwnerUserId(user);
    } catch (const exception &e) {
        LOG(ERROR) << "Failed to set owner user ID: " << e.what();
    }
}

TgBotDatabaseImpl::AddResult TgBotDatabaseImpl::addChatInfo(
    const ChatId chatid, const std::string_view name) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return AddResult::BACKEND_ERROR;
    }
    try {
        return _databaseImpl->addChatInfo(chatid, name);
    } catch (const exception& ex) {
        LOG(ERROR) << "Failed to add chat info: " << ex.what();
        return AddResult::BACKEND_ERROR;
    }
}

std::optional<ChatId> TgBotDatabaseImpl::getChatId(
    const std::string_view name) const {
    if (!isLoaded()) {
        LOG(ERROR) << __func__ << ": No-op due to missing database";
        return std::nullopt;
    }
    try {
        return _databaseImpl->getChatId(name);
    } catch (const exception& ex) {
        LOG(ERROR) << "Failed to get chat ID: " << ex.what();
        return std::nullopt;
    }
}

TgBotDatabaseImpl::Providers::Providers(CommandLine* cmdline) {
#ifdef DATABASE_HAVE_SQLITE
    registerProvider("sqlite", std::make_unique<SQLiteDatabase>(
        cmdline->getPath(FS::PathType::RESOURCES_SQL)
    ));
#else
    (void)cmdline;
#endif
#ifdef DATABASE_HAVE_PROTOBUF
    registerProvider("protobuf", std::make_unique<ProtoDatabase>());
#endif
}

void TgBotDatabaseImpl::Providers::registerProvider(
    const std::string_view name, std::unique_ptr<DatabaseBase> provider) {
    // Check if the provider has already been registered
    if (_providers.contains(name)) {
        LOG(WARNING) << fmt::format(
            "Database provider with name '{}' already registered.", name);
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

bool TgBotDatabaseImpl::Providers::chooseAnyProvider() {
    if (!_providers.empty()) {
        // Move the first provider to chosenProvider and clear the map
        auto first = _providers.begin();
        chosenProvider = std::move(first->second);
        DLOG(INFO) << "Chosen any database provider: " << first->first;
        _providers.clear();
        return true;
    } else {
        chosenProvider = nullptr;
        LOG(WARNING) << "No database providers registered.";
        return false;
    }
}