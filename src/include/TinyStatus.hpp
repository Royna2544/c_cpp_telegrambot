#pragma once

#include <string>

namespace tinystatus {

enum class Status {
    kOk,
    kInvalidImage,
    kInvalidArgument,
    kReadError,
    kWriteError,
    kProcessingError,
    kInternalError,
    kUnimplemented,
    kUnknown,
};

class TinyStatus {
    Status error;
    std::string message;

   public:
    explicit TinyStatus(Status s) : error(s) {}
    TinyStatus(Status s, std::string msg) : error(s), message(std::move(msg)) {}
    explicit operator Status() const { return error; }

    // Accessor methods
    [[nodiscard]] bool isOk() const { return error == Status::kOk; }
    [[nodiscard]] Status status() const { return error; }
    [[nodiscard]] const std::string& getMessage() const { return message; }

    // Factory method for OK status
    [[nodiscard]] static TinyStatus ok() { return TinyStatus(Status::kOk); }
};

}  // namespace tinystatus