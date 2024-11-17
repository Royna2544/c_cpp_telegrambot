#pragma once

#include <absl/log/log.h>
#include <absl/status/status.h>

#include <filesystem>
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
    virtual absl::Status read(const std::filesystem::path& filename,
                              Target target = Target::kNone) = 0;

    /**
     * @brief Processes and writes the image to the specified file.
     *
     * This function applies the specified options (if any) to the image and
     * writes the processed image to the specified file. The options include
     * rotation, greyscale conversion, color inversion, and destination file
     * path.
     *
     * @param[in] filename The path to the output image file. If the destination
     * option is set, this parameter is ignored.
     *
     * @return An absl::Status indicating the success or failure of the
     * operation.
     * - absl::StatusCode::kOk: The operation was successful.
     * - absl::StatusCode::kInvalidArgument: Invalid input parameters.
     * - absl::StatusCode::kFailedPrecondition: The image is not valid or cannot
     * be processed.
     * - absl::StatusCode::kUnknown: An unknown error occurred during the
     * operation.
     *
     * @note The function does not handle cases where the image is not valid or
     *       cannot be processed.
     */
    virtual absl::Status processAndWrite(
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