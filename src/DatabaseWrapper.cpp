#include <Database.h>
#include <SingleThreadCtrl.h>

using database::DatabaseWrapper;

struct DatabaseSync : SingleThreadCtrl {
    void run() {
        using_cv = true;
        while (kRun) {
            database::DBWrapper.save();
            std::unique_lock<std::mutex> lk(CV_m);
            cv.wait_for(lk, std::chrono::seconds(10));
        }
    }
};

void DatabaseWrapper::load() {
    std::call_once(once, [this] {
        fname = getDatabaseFile().string();
        std::fstream input(fname, std::ios::in | std::ios::binary);
        if (!input)
            throw std::runtime_error("Failed to load database file");
        protodb.ParseFromIstream(&input);
        protomediadb.ParseFromIstream(&input);
        auto syncMgr = gSThreadManager.getController<DatabaseSync>
            (SingleThreadCtrlManager::USAGE_DATABASE_SYNC_THREAD);
        syncMgr->runWith(std::bind(&DatabaseSync::run, syncMgr));
        loaded = true;
    });
}

void DatabaseWrapper::save() const {
    if (warnNoLoaded(__func__)) {
        std::fstream output(fname, std::ios::out | std::ios::trunc | std::ios::binary);
        assert(output);
        protodb.SerializeToOstream(&output);
        protomediadb.SerializePartialToOstream(&output);
    }
}

UserId DatabaseWrapper::maybeGetOwnerId() const {
    if (warnNoLoaded(__func__) && protodb.has_ownerid())
        return protodb.ownerid();
    else
        return -1;
}

bool DatabaseWrapper::warnNoLoaded(const char* func) const {
    if (!loaded) {
        LOG_W("Database not loaded! Called function: '%s'", func);
    }
    return loaded;
}

namespace database {
DatabaseWrapper DBWrapper;
} // namespace database