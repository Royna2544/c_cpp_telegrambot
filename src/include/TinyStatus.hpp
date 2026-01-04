#pragma once

#include <ostream>
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

inline std::ostream& operator<<(std::ostream& os, const tinystatus::Status s) {
    switch (s) {
        case tinystatus::Status::kOk:
            return os << "OK";
        case tinystatus::Status::kInvalidImage:
            return os << "Invalid Image";
        case tinystatus::Status::kInvalidArgument:
            return os << "Invalid Argument";
        case tinystatus::Status::kReadError:
            return os << "Read Error";
        case tinystatus::Status::kWriteError:
            return os << "Write Error";
        case tinystatus::Status::kProcessingError:
            return os << "Processing Error";
        case tinystatus::Status::kInternalError:
            return os << "Internal Error";
        case tinystatus::Status::kUnimplemented:
            return os << "Unimplemented";
        case tinystatus::Status::kUnknown:
            return os << "Unknown";
    }
    return os;
}

inline std::ostream& operator<<(std::ostream& os,
                                const tinystatus::TinyStatus& ts) {
    os << ts.status();
    if (!ts.getMessage().empty()) {
        os << " (" << ts.getMessage() << ")";
    }
    return os;
}