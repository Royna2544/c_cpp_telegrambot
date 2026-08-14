#pragma once

#include <absl/log/log.h>

#include <TinyStatus.hpp>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <stop_token>
#include <string>
#include <trivial_helpers/generic_opt.hpp>

template <int Min, int Max>
class RangeRestricted {
    static_assert(Min <= Max,
                  "Low value must be less than or equal to Max value");

    int _value;

   public:
    RangeRestricted(int value) : _value(value) {
        constexpr int range = Max - Min;
        static_assert(range > 0, "RangeRestricted requires a non-empty range");
        if (_value < Min || _value >= Max) {
            const auto offset = static_cast<std::int64_t>(_value) - Min;
            _value = static_cast<int>(((offset % range) + range) % range + Min);
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

    using Status = tinystatus::Status;
    using TinyStatus = tinystatus::TinyStatus;

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

    struct ProcessingControl {
        std::stop_token stop;
        std::chrono::steady_clock::time_point deadline =
            std::chrono::steady_clock::time_point::max();

        [[nodiscard]] bool shouldStop() const noexcept {
            return stop.stop_requested() ||
                   std::chrono::steady_clock::now() >= deadline;
        }
    } processingControl;

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
