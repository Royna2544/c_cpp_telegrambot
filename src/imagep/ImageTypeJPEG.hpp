#pragma once

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

    bool read(const std::filesystem::path& filename) override;
    Result _rotate_image(int angle) override;
    void to_greyscale() override;
    bool write(const std::filesystem::path& filename) override;
    std::string version() const override;

   private:
    std::unique_ptr<unsigned char[]> image_data;
    size_t width{};
    size_t height{};
    int num_channels{};
};