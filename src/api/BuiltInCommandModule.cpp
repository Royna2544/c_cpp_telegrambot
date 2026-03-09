#include <absl/log/log.h>

#include "api/CommandModule.hpp"

BuiltinCommandModule::BuiltinCommandModule(const DynModule* dyn) {
    LOG(INFO) << "Initializing built-in command module: " << dyn->name;
    info = Info(dyn);
}

bool BuiltinCommandModule::load() {
    loaded = true;
    return true;
}

bool BuiltinCommandModule::unload() {
    loaded = false;
    return true;
}

// Trival accessors.
[[nodiscard]] bool BuiltinCommandModule::isLoaded() const {
    return loaded;
}