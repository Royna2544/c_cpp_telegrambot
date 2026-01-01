#pragma once

#include <SharedMalloc.hpp>
#include <unordered_map>

#include <trivial_helpers/fruit_inject.hpp>

// Global storage for maintaining application-wide state for command modules.
class GlobalStorage {
    std::unordered_map<std::string, SharedMalloc> storage_map_;

   public:

    APPLE_INJECT(GlobalStorage()) = default;

    // Retrieve or create a shared storage object by key.
    SharedMalloc& getStorage(const std::string& key) {
        return storage_map_[key];
    }

    // Remove a storage object by key.
    void removeStorage(const std::string& key) { storage_map_.erase(key); }

    // Clear all storage objects.
    void clearAll() { storage_map_.clear(); }

    SharedMalloc& operator[](const std::string& key) { return getStorage(key); }
};