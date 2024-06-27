#include "StringResManager.hpp"

#include <ConfigManager.h>
#include <libos/libfs.hpp>
#include "InstanceClassBase.hpp"

void StringResManager::doInitCall() {
    auto locale = getVariable(ConfigManager::Configs::LOCALE);
    if (!locale) {
        LOG(WARNING) << "Using default locale: en-US";
        locale = "en-US";
    }
    bool res = parseFromFile(FS::getPathForType(FS::PathType::RESOURCES) /
                                 "strings" / locale->append(".xml"),
                             STRINGRES_MAX);
    if (!res) {
        LOG(ERROR) << "Failed to parse string res, abort";
    }
}

const CStringLifetime StringResManager::getInitCallName() const {
    return "Load language resource file";
}

DECLARE_CLASS_INST(StringResManager);