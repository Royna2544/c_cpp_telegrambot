#pragma once

#include <absl/log/log.h>

#include <filesystem>
#include <stdexcept>
#include <string>
#include <trivial_helpers/generic_opt.hpp>

template <int Min, int Max>
class RangeRestricted {
    static_assert(Min <= Max,
                  "Low value must be less than or equal to Max value");

    int _value;

   public:
    RangeRestricted(int value) : _value(value) {
        if (_value < Min || _value > Max) {
            _value %= Max - Min;
        }
    }
    operator int() const { return _value; }
};

/**
 * @brief Base class for photo manipulation.
 *
 * This class provides a base for photo manipulation operations such as reading,
 * writing, and transforming images.
 */
struct PhotoBase {
    static constexpr int kAngleMin = 0;
    static constexpr int kAngle90 = 90;
    static constexpr int kAngle180 = 180;
    static constexpr int kAngle270 = 270;
    static constexpr int kAngleMax = 360;

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
        TinyStatus(Status s, std::string msg)
            : error(s), message(std::move(msg)) {}
        explicit operator Status() const { return error; }

        // Accessor methods
        [[nodiscard]] bool isOk() const { return error == Status::kOk; }
        [[nodiscard]] Status status() const { return error; }
        [[nodiscard]] const std::string& getMessage() const { return message; }

        // Factory method for OK status
        [[nodiscard]] static TinyStatus ok() { return TinyStatus(Status::kOk); }
    };

    enum class Target {
        kNone,
        kVideo,
        kPhoto,
    };

    template <typename T>
    using Option = generic_opt::Option<T>;

    struct Options {
        Option<RangeRestricted<kAngleMin, kAngleMax>> rotate_angle{0};
        Option<bool> greyscale;
        Option<bool> invert_color;
    } options;

    /**
     * @brief Reads an image from the specified file.
     *
     * @param[in] filename The path to the image file.
     * @param[in] target Target specification for the image reading process.
     * @return The result of the read operation
     */
    virtual TinyStatus read(const std::filesystem::path& filename,
                            Target target = Target::kNone) = 0;

    /**
     * @brief Processes and writes the image to the specified file.
     *
     * This function applies the specified options (if any) to the image and
     * writes the processed image to the specified file. The options include
     * rotation, greyscale conversion, color inversion, and destination file
     * path.
     *
     * @param[in] filename The path to the output image file.
     *
     * @return A TinyStatus indicating the success or failure of the
     * operation.
     * - Status::kOk: The operation was successful.
     * - Status::kWriteError: Failed to write the image to the specified file.
     * - Status::kReadError: Failed to read the image from the source file.
     * - Status::kInvalidArgument: Invalid input parameters.
     * - Status::kProcessingError: The image is not valid or cannot
     * be processed.
     * - Status::kUnknown: An unknown error occurred during the
     * operation.
     *
     * @note The function does not handle cases where the image is not valid or
     *       cannot be processed.
     */
    virtual TinyStatus processAndWrite(
        const std::filesystem::path& filename) = 0;

    /**
     * @brief Destructor for the photo manipulation base class.
     */
    virtual ~PhotoBase() = default;

    [[nodiscard]] virtual std::string version() const = 0;
};

inline std::ostream& operator<<(std::ostream& os, const PhotoBase::Target t) {
    switch (t) {
        case PhotoBase::Target::kNone:
            return os << "none";
        case PhotoBase::Target::kVideo:
            return os << "video";
        case PhotoBase::Target::kPhoto:
            return os << "photo";
    }
    return os;
}

inline std::ostream& operator<<(std::ostream& os, const PhotoBase::Status s) {
    switch (s) {
        case PhotoBase::Status::kOk:
            return os << "OK";
        case PhotoBase::Status::kInvalidImage:
            return os << "Invalid Image";
        case PhotoBase::Status::kInvalidArgument:
            return os << "Invalid Argument";
        case PhotoBase::Status::kReadError:
            return os << "Read Error";
        case PhotoBase::Status::kWriteError:
            return os << "Write Error";
        case PhotoBase::Status::kProcessingError:
            return os << "Processing Error";
        case PhotoBase::Status::kInternalError:
            return os << "Internal Error";
        case PhotoBase::Status::kUnimplemented:
            return os << "Unimplemented";
        case PhotoBase::Status::kUnknown:
            return os << "Unknown";
    }
    return os;
}

inline std::ostream& operator<<(std::ostream& os,
                                const PhotoBase::TinyStatus& ts) {
    os << ts.status();
    if (!ts.getMessage().empty()) {
        os << " (" << ts.getMessage() << ")";
    }
    return os;
}