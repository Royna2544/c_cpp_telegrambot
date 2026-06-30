#pragma once

#include <mutex>
#include <string>
#include <utility>

namespace tgbot::builder {

/**
 * @brief Thread-safe, transport-agnostic build lifecycle state.
 *
 * Tracks whether a build is running and its current id. Shared by both
 * builders (previously each module carried its own ad-hoc copy).
 */
class BuildState {
   public:
    void start(std::string buildId) {
        const std::lock_guard<std::mutex> lock(mutex_);
        id_ = std::move(buildId);
        running_ = true;
    }

    void finish() {
        const std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
    }

    [[nodiscard]] bool running() const {
        const std::lock_guard<std::mutex> lock(mutex_);
        return running_;
    }

    [[nodiscard]] std::string id() const {
        const std::lock_guard<std::mutex> lock(mutex_);
        return id_;
    }

    void setId(std::string buildId) {
        const std::lock_guard<std::mutex> lock(mutex_);
        id_ = std::move(buildId);
    }

   private:
    mutable std::mutex mutex_;
    std::string id_;
    bool running_ = false;
};

}  // namespace tgbot::builder
