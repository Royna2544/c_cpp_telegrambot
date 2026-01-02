#pragma once

#include <SharedMalloc.hpp>
#include <unordered_map>
#include <mutex>

#include <trivial_helpers/fruit_inject.hpp>

// Global storage for maintaining application-wide state for command modules.
class GlobalStorage {
    std::unordered_map<std::string, SharedMalloc> storage_map_;
    mutable std::mutex storage_mutex_;

   public:

    APPLE_INJECT(GlobalStorage()) = default;

    // Retrieve or create a shared storage object by key.
    SharedMalloc& getStorage(const std::string& key) {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        return storage_map_[key];
    }

    // Remove a storage object by key.
    void removeStorage(const std::string& key) { 
        std::lock_guard<std::mutex> lock(storage_mutex_);
        storage_map_.erase(key); 
    }
    
    // Check if a storage object exists by key.
    bool hasStorage(const std::string& key) const {
        std::lock_guard<std::mutex> lock(storage_mutex_);
        return storage_map_.find(key) != storage_map_.end();
    }
    
    // Clear all storage objects.
    void clearAll() { 
        std::lock_guard<std::mutex> lock(storage_mutex_);
        storage_map_.clear(); 
    }

    SharedMalloc& operator[](const std::string& key) { return getStorage(key); }
};