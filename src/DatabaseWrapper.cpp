#include <Database.h>
#include <SingleThreadCtrl.h>

#include <filesystem>
#include <memory>

#include "Logging.h"

using database::DatabaseWrapper;

struct DatabaseSync : SingleThreadCtrlRunnable<> {
    void runFunction() override {
        while (kRun) {
            database::DBWrapper.save();
            delayUnlessStop(10);
        }
    }
};

void DatabaseWrapper::load(const std::filesystem::path& it) {
    std::call_once(once, [this, it] {
        fname = it;
        std::fstream input(fname.string(), std::ios::in | std::ios::binary);
        if (!input) throw std::runtime_error("Failed to load database file");
        protodb.ParseFromIstream(&input);
        loaded = true;
    });
}

void DatabaseWrapper::load() {
    load(getSrcRoot() / std::string(kDatabaseFile));
}

void DatabaseWrapper::loadMain(const Bot& bot) {
    load();
    const SingleThreadCtrlManager::GetControllerRequest req{
        .usage = SingleThreadCtrlManager::USAGE_DATABASE_SYNC_THREAD,
        .flags = SingleThreadCtrlManager::GetControllerFlags::REQUIRE_NONEXIST |
                 SingleThreadCtrlManager::GetControllerFlags::
                     REQUIRE_FAILACTION_RETURN_NULL};
    if (const auto it = gSThreadManager.getController<DatabaseSync>(req); it)
        it->run();

    whitelist = std::make_shared<ProtoDatabase>(
        bot, "whitelist", protodb.mutable_whitelist()->mutable_id());
    blacklist = std::make_shared<ProtoDatabase>(
        bot, "blacklist", protodb.mutable_blacklist()->mutable_id());
    whitelist->setOtherProtoDatabaseBase(blacklist);
    blacklist->setOtherProtoDatabaseBase(whitelist);
}

void DatabaseWrapper::save() const {
    if (warnNoLoaded(__func__)) {
        std::fstream output(fname,
                            std::ios::out | std::ios::trunc | std::ios::binary);
        assert(output);
        protodb.SerializeToOstream(&output);
    }
}

UserId DatabaseWrapper::maybeGetOwnerId() const {
    if (warnNoLoaded(__func__) && protodb.has_ownerid())
        return protodb.ownerid();
    LOG(LogLevel::WARNING,
        "No owner id found in database, enforced commands will not work");
    return -1;
}

bool DatabaseWrapper::warnNoLoaded(const char* func) const {
    if (!loaded) {
        LOG(LogLevel::WARNING, "Database not loaded! Called function: '%s'",
            func);
    }
    return loaded;
}

namespace database {
DatabaseWrapper DBWrapper;
}  // namespace database