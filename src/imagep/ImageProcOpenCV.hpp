#pragma once

#include "ImagePBase.hpp"

#include <opencv2/opencv.hpp>

/**
 * @brief Derived class for photo manipulation using OpenCV.
 *
 * This class provides implementation for photo manipulation operations using
 * OpenCV.
 */
class OpenCVImage : public PhotoBase {
   public:
    OpenCVImage() = default;
    ~OpenCVImage() override = default;

    bool read(const std::filesystem::path& filename) override;
    Result _rotate_image(int angle) override;
    void to_greyscale() override;
    bool write(const std::filesystem::path& filename) override;

   private:
    cv::Mat image;
};