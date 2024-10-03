#pragma once

#include <absl/log/log.h>
#include <internal/_class_helper_macros.h>

#include <DurationPoint.hpp>
#include <iomanip>
#include <string_view>

// A tag struct for enabling initialization. Accessing private members
// A class that wants to implement initialization would set the operator<<
// For example,
// class MyClass {
// public:
// friend InitTask& operator<<(InitTask& tag, NetworkLogSink& thiz);
// };

struct InitFailed_t {};
struct InitSuccess_t {};

// A global constant for InitFailed
inline constexpr InitFailed_t InitFailed;
// A global constant for InitSuccess
inline constexpr InitSuccess_t InitSuccess;

struct InitTask {
    InitTask& operator<<(const std::string_view name) {
        LOG(INFO) << "Starting task: " << std::quoted(name);
        dp.init();
        return *this;
    }
    InitTask& operator<<(const char *__restrict name) {
        return operator<<(std::string_view(name));
    }

    InitTask& operator<<(InitFailed_t /*fail*/) {
        LOG(WARNING) << "Failed, took " << dp.get().count() << "ms";
        return *this;
    }

    InitTask& operator<<(InitSuccess_t /*success*/) {
        LOG(INFO) << "Done, took " << dp.get().count() << "ms";
        return *this;
    }

    InitTask& operator<<(bool success) {
        if (success) {
            return *this << InitSuccess;
        } else {
            return *this << InitFailed;
        }
    }

    InitTask() = default;
    ~InitTask() = default;

    NO_COPY_CTOR(InitTask);
    NO_MOVE_CTOR(InitTask);

   private:
    DurationPoint dp;
};