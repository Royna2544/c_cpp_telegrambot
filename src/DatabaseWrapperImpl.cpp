#include <Database.h>
#include "InstanceClassBase.hpp"

void database::DatabaseWrapperImpl::load() {
    DatabaseWrapper::load(FS::getPathForType(FS::PathType::GIT_ROOT) /
                          std::string(kDatabaseFile));
}

DECLARE_CLASS_INST(database::DatabaseWrapperImplObj);