#pragma once

#include <absl/status/status.h>

#include "ImagePBase.hpp"

/**
 * @brief Derived class for photo manipulation using libjpeg.
 *
 * This class provides implementation for photo manipulation operations using
 * libjpeg.
 */
class JPEGImage : public PhotoBase {
   public:
    JPEGImage() noexcept = default;
    ~JPEGImage() override = default;

    absl::Status read(const std::filesystem::path& filename,
                      Target target = Target::kNone) override;
    absl::Status processAndWrite(
        const std::filesystem::path& filename) override;
    std::string version() const override;

   private:
    absl::Status rotate(int angle);
    void greyscale();
    void invert();
    std::unique_ptr<unsigned char[]> image_data;
    size_t width{};
    size_t height{};
    int num_channels{};
    static constexpr int QUALITY = 95;
};