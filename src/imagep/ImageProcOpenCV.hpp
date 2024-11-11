#pragma once

#include <absl/status/status.h>

#include <filesystem>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include "ImagePBase.hpp"

/**
 * @brief Derived class for photo manipulation using OpenCV.
 *
 * This class provides implementation for photo manipulation operations using
 * OpenCV.
 */
class OpenCVImage : public PhotoBase {
   public:
    OpenCVImage() noexcept = default;
    ~OpenCVImage() override = default;

    absl::Status read(const std::filesystem::path& filename,
                      Target flags) override;
    absl::Status processAndWrite(
        const std::filesystem::path& filename) override;
    std::string version() const override;

   private:
    struct ComponentBase {
        virtual ~ComponentBase() = default;
        virtual absl::Status read(const std::filesystem::path& file) = 0;
        virtual absl::Status procAndW(
            const Options* opt, const std::filesystem::path& dest) = 0;
    };
    
    struct Image : public ComponentBase {
        cv::Mat handle;

        absl::Status read(const std::filesystem::path& file) override;
        static void rotate(cv::Mat& mat, int angle);
        static void greyscale(cv::Mat& mat);
        static void invert(cv::Mat& mat);

        absl::Status procAndW(const Options* opt,
                              const std::filesystem::path& dest) override;
    };
    struct Video : public ComponentBase {
        cv::VideoCapture handle;
        int fourcc{};

        absl::Status read(const std::filesystem::path& file) override;
        absl::Status procAndW(const Options* opt,
                              const std::filesystem::path& filename) override;
    };
    std::unique_ptr<ComponentBase> component;
};