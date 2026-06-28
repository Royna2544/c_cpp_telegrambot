#include <absl/log/log.h>

#include <atomic>

#include "api/CommandModule.hpp"

BuiltinCommandModule::BuiltinCommandModule(const DynModule* dyn) {
    LOG(INFO) << "Initializing built-in command module: " << dyn->name;
    info = Info(dyn);
}

bool BuiltinCommandModule::load() {
    bool expected = false;
    return loaded.compare_exchange_strong(expected, true);
}

bool BuiltinCommandModule::unload() {
    bool expected = true;
    return loaded.compare_exchange_strong(expected, false);
}

// Trival accessors.
[[nodiscard]] bool BuiltinCommandModule::isLoaded() const {
    return loaded.load(std::memory_order_relaxed);
}