#include <Database.h>
#include <SingleThreadCtrl.h>
#include <libos/libfs.hpp>

#include <filesystem>

#include <absl/log/log.h>

using database::DatabaseWrapper;

void database::DatabaseWrapperImpl::load() {
    DatabaseWrapper::load(FS::getPathForType(FS::PathType::GIT_ROOT) /
                          std::string(kDatabaseFile));
}

DECLARE_CLASS_INST(database::DatabaseWrapperImplObj);

void DatabaseWrapper::load(const std::filesystem::path& it) {
    std::call_once(once, [this, it] {
        fname = it;
        std::fstream input(fname.string(), std::ios::in | std::ios::binary);
        if (!input) throw std::runtime_error("Failed to load database file");
        protodb.ParseFromIstream(&input);
        loaded = true;
    });
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
    LOG(WARNING) << 
        "No owner id found in database, enforced commands will not work";
    return -1;
}

bool DatabaseWrapper::warnNoLoaded(const char* func) const {
    if (!loaded) {
        LOG(WARNING) << "Database not loaded! Called function: " << func;
    }
    return loaded;
}
