#include "ImageProcOpenCV.hpp"

#include <absl/status/status.h>

#include <filesystem>
#include <memory>
#include <opencv2/core.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>

absl::Status OpenCVImage::read(const std::filesystem::path& filename,
                               const Target flags) {
    switch (flags) {
        case PhotoBase::Target::kPhoto: {
            component = std::make_unique<Image>();
            auto ret = component->read(filename);
            if (!ret.ok()) {
                LOG(ERROR) << "Error reading image: " << ret;
                return ret;
            }
            return absl::OkStatus();
        }
        case PhotoBase::Target::kVideo: {
            component = std::make_unique<Video>();
            auto ret = component->read(filename);
            if (!ret.ok()) {
                return ret;
            }
            return absl::OkStatus();
        }
        case PhotoBase::Target::kNone:
            LOG(ERROR) << "Invalid target type";
            return absl::UnavailableError("Invalid target type");
    }
}

void OpenCVImage::Image::rotate(cv::Mat& mat, int angle) {
    // Make it clockwise
    angle = kAngleMax - angle;

    cv::Point2d center(mat.cols / 2.0, mat.rows / 2.0);
    cv::Mat rotation_matrix = cv::getRotationMatrix2D(center, angle, 1.0);

    // Calculate the bounding box of the rotated image
    cv::Rect bbox =
        cv::RotatedRect(cv::Point2f(), mat.size(), angle).boundingRect();

    // Adjust the rotation matrix to take into account translation
    rotation_matrix.at<double>(0, 2) += bbox.width / 2.0 - center.x;
    rotation_matrix.at<double>(1, 2) += bbox.height / 2.0 - center.y;

    cv::Mat rotated_image;
    cv::warpAffine(mat, rotated_image, rotation_matrix, bbox.size());

    std::swap(mat, rotated_image);
}

void OpenCVImage::Image::greyscale(cv::Mat& mat) {
    if (mat.channels() == 3) {
        // Convert a BGR image to grayscale
        cv::Mat gray_image;
        cv::cvtColor(mat, gray_image, cv::COLOR_BGR2GRAY);
        gray_image.copyTo(mat);  // Update the image with the grayscale data
    } else if (mat.channels() == 4) {
        // Convert a BGRA image to grayscale while preserving the alpha channel
        cv::Mat gray_image;
        cv::Mat alpha_channel;
        std::vector<cv::Mat> channels(4);

        // Split the channels
        cv::split(mat, channels);
        cv::cvtColor(mat, gray_image, cv::COLOR_BGRA2GRAY);

        // Extract the alpha channel
        alpha_channel = channels[3];

        // Merge grayscale and alpha channel back into one image
        cv::Mat gray_alpha_image;
        std::vector<cv::Mat> gray_alpha_channels = {gray_image, gray_image,
                                                    gray_image, alpha_channel};
        cv::merge(gray_alpha_channels, gray_alpha_image);
        // Update the image with the grayscale data and preserved alpha channel
        gray_alpha_image.copyTo(mat);
    } else {
        LOG(INFO) << "Image does not have enough color channels to convert to "
                     "grayscale.";
    }
}

void OpenCVImage::Image::invert(cv::Mat& mat) {
    cv::Mat inverted_image;
    cv::bitwise_not(mat, inverted_image);
    mat = inverted_image;
}

absl::Status OpenCVImage::Image::read(const std::filesystem::path& file) {
    handle = cv::imread(file.string(), cv::IMREAD_UNCHANGED);
    if (handle.empty()) {
        LOG(ERROR) << "Error reading image: " << file;
        return absl::InternalError("Error reading image");
    }
    LOG(INFO) << "Image dimensions: " << handle.cols << "x" << handle.rows;
    return absl::OkStatus();
}

absl::Status OpenCVImage::Video::read(const std::filesystem::path& file) {
    handle = cv::VideoCapture(file.string(), cv::CAP_FFMPEG);
    if (!handle.isOpened()) {
        LOG(ERROR) << "Error opening video: " << file;
        return absl::InternalError("Error opening video");
    }
    LOG(INFO) << "Video dimensions: " << handle.get(cv::CAP_PROP_FRAME_WIDTH)
              << "x" << handle.get(cv::CAP_PROP_FRAME_HEIGHT);
    return absl::OkStatus();
}

absl::Status OpenCVImage::Video::procAndW(const Options* opt,
                                          const std::filesystem::path& dest) {
    if (!handle.isOpened()) {
        LOG(ERROR) << "No video data to rotate";
        return absl::NotFoundError("No video data to rotate");
    }

    int fourcc = 0;
    if (dest.extension() == ".mp4") {
        fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    } else if (dest.extension() == ".webm") {
        fourcc = cv::VideoWriter::fourcc('V', 'P', '8', '0');
    } else {
        LOG(ERROR) << "Unsupported output file format: " << dest.extension();
        return absl::InvalidArgumentError("Unsupported output file format");
    }

    // Get original video properties
    int frame_width = static_cast<int>(handle.get(cv::CAP_PROP_FRAME_WIDTH));
    int frame_height = static_cast<int>(handle.get(cv::CAP_PROP_FRAME_HEIGHT));
    int fps = static_cast<int>(handle.get(cv::CAP_PROP_FPS));

    // Set up VideoWriter
    auto outVidSize = cv::Size(frame_width, frame_height);
    cv::VideoWriter writer(dest, fourcc, fps, outVidSize);

    if (!writer.isOpened()) {
        LOG(ERROR) << "Could not open the output video file for write";
        return absl::InternalError("Failed to open the output video file");
    }

    // Rotate each frame and write it to the output video
    cv::Mat frame;
    while (true) {
        handle >> frame;
        if (frame.empty()) {
            break;  // End of video
        }

        // Rotate the frame
        Image::rotate(frame, opt->rotate_angle.get());
        // Convert the frame to grayscale
        if (opt->greyscale.get()) {
            Image::greyscale(frame);
        }

        if (opt->invert_color.get()) {
            Image::invert(frame);
        }

        if (frame.size[0] != outVidSize.width ||
            frame.size[1] != outVidSize.height) {
            cv::resize(frame, frame, outVidSize);
        }

        // Write the rotated frame to the output video
        writer << frame;
    }
    return absl::OkStatus();
}

absl::Status OpenCVImage::Image::procAndW(const Options* opt,
                                          const std::filesystem::path& dest) {
    if (handle.empty()) {
        return absl::NotFoundError("No image data to process");
    }

    // Convert the image to grayscale if required
    if (opt->greyscale.get()) {
        Image::greyscale(handle);
    }

    if (opt->invert_color.get()) {
        Image::invert(handle);
    }

    Image::rotate(handle, opt->rotate_angle.get());

    // Write the processed image to the output file
    try {
        if (!cv::imwrite(dest, handle)) {
            return absl::InternalError("Failed to write image to: " +
                                       dest.string());
        }
    } catch (const cv::Exception& ex) {
        LOG(ERROR) << "Error writing image: " << ex.what();
        return absl::InternalError("Error writing image");
    }
    return absl::OkStatus();
}

absl::Status OpenCVImage::processAndWrite(
    const std::filesystem::path& filename) {
    if (!component) {
        LOG(ERROR) << "No image or video component loaded";
        return absl::NotFoundError("No image or video component loaded");
    }
    return component->procAndW(&options, filename);
}

std::string OpenCVImage::version() const {
    return "OpenCV version: " + cv::getVersionString();
}