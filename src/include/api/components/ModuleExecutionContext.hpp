#pragma once

#include <string>
#include <string_view>
#include <utility>

namespace module_execution {

inline thread_local std::string activeOwner;

class Scope {
   public:
    explicit Scope(std::string owner) : previous_(std::move(activeOwner)) {
        activeOwner = std::move(owner);
    }

    ~Scope() { activeOwner = std::move(previous_); }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;

   private:
    std::string previous_;
};

inline bool isExecuting(std::string_view owner) {
    return activeOwner == owner;
}

inline std::string_view currentOwner() {
    return activeOwner;
}

}  // namespace module_execution
