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

    TinyStatus read(const std::filesystem::path& filename,
                    Target target = Target::kNone) override;
    TinyStatus processAndWrite(const std::filesystem::path& filename) override;
    [[nodiscard]] std::string version() const override;

   private:
    TinyStatus rotate(int angle);
    void greyscale();
    void invert();
    std::unique_ptr<unsigned char[]> image_data;
    size_t width{};
    size_t height{};
    int num_channels{};
    static constexpr int QUALITY = 95;
};