#pragma once

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

    TinyStatus read(const std::filesystem::path& filename,
                    Target flags) override;
    TinyStatus processAndWrite(const std::filesystem::path& filename) override;
    [[nodiscard]] std::string version() const override;

   private:
    struct ComponentBase {
        virtual ~ComponentBase() = default;
        virtual TinyStatus read(const std::filesystem::path& file) = 0;
        virtual TinyStatus procAndW(const Options* opt,
                                    const std::filesystem::path& dest) = 0;
    };

    struct Image : public ComponentBase {
        cv::Mat handle;

        TinyStatus read(const std::filesystem::path& file) override;
        static void rotate(cv::Mat& mat, int angle);
        static void greyscale(cv::Mat& mat);
        static void invert(cv::Mat& mat);

        TinyStatus procAndW(const Options* opt,
                            const std::filesystem::path& dest) override;
    };
    struct Video : public ComponentBase {
        cv::VideoCapture handle;

        TinyStatus read(const std::filesystem::path& file) override;
        TinyStatus procAndW(const Options* opt,
                            const std::filesystem::path& filename) override;
    };
    std::unique_ptr<ComponentBase> component;
};