#include "ImageProcOpenCV.hpp"

#include <fmt/format.h>

#include <cmath>
#include <filesystem>
#include <memory>
#include <opencv2/core.hpp>
#include <opencv2/core/utility.hpp>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/videoio.hpp>

#include "MediaLimits.hpp"

OpenCVImage::TinyStatus OpenCVImage::read(const std::filesystem::path& filename,
                                          const Target flags) {
    switch (flags) {
        case PhotoBase::Target::kPhoto: {
            component = std::make_unique<Image>();
            return component->read(filename);
        }
        case PhotoBase::Target::kVideo: {
            component = std::make_unique<Video>();
            return component->read(filename);
        }
        case PhotoBase::Target::kNone:
            break;
    }
    return {Status::kInvalidArgument, "Invalid target type"};
}

void OpenCVImage::Image::rotate(cv::Mat& mat, int angle) {
    // Adjust angle to be clockwise
    angle = -angle;

    cv::Point2d center(mat.cols / 2.0, mat.rows / 2.0);

    // Compute the rotation matrix
    cv::Mat rotation_matrix = cv::getRotationMatrix2D(center, angle, 1.0);

    // Calculate bounding box with the center of the image
    cv::Rect bbox = cv::RotatedRect(center, mat.size(), angle).boundingRect();

    // Adjust rotation matrix for translation to keep the image centered
    rotation_matrix.at<double>(0, 2) += (bbox.width / 2.0) - center.x;
    rotation_matrix.at<double>(1, 2) += (bbox.height / 2.0) - center.y;

    // Apply the rotation with adjusted matrix and bounding box size
    cv::Mat rotated_image;
    cv::warpAffine(mat, rotated_image, rotation_matrix, bbox.size(),
                   cv::INTER_LINEAR, cv::BORDER_REFLECT);

    // Copy rotated image back to the original
    mat = rotated_image;
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
    if (mat.channels() != 4) {
        cv::bitwise_not(mat, mat);
        return;
    }

    // Preserve transparency. Inverting alpha makes transparent pixels opaque
    // and is inconsistent with the native PNG/WebP implementations.
    std::vector<cv::Mat> channels;
    cv::split(mat, channels);
    for (std::size_t i = 0; i < 3; ++i) {
        cv::bitwise_not(channels[i], channels[i]);
    }
    cv::merge(channels, mat);
}

OpenCVImage::TinyStatus OpenCVImage::Image::read(
    const std::filesystem::path& file) {
    handle = cv::imread(file.string(), cv::IMREAD_UNCHANGED);
    if (handle.empty()) {
        return {Status::kInternalError,
                "Error opening image: " + file.filename().string()};
    }
    LOG(INFO) << "Image dimensions: " << handle.cols << "x" << handle.rows;
    return {Status::kOk, "Image read successfully"};
}

OpenCVImage::TinyStatus OpenCVImage::Video::read(
    const std::filesystem::path& file) {
    handle = cv::VideoCapture(file.string());
    if (!handle.isOpened()) {
        return {Status::kInternalError,
                "Error opening video file: " + file.filename().string()};
    }
    const auto width = handle.get(cv::CAP_PROP_FRAME_WIDTH);
    const auto height = handle.get(cv::CAP_PROP_FRAME_HEIGHT);
    const auto fps = handle.get(cv::CAP_PROP_FPS);
    const auto frameCount = handle.get(cv::CAP_PROP_FRAME_COUNT);
    if (!std::isfinite(width) || !std::isfinite(height) ||
        !imagep::limits::videoDimensionsAllowed(
            static_cast<std::int64_t>(width),
            static_cast<std::int64_t>(height)) ||
        !std::isfinite(fps) || fps <= 0.0 ||
        fps > static_cast<double>(imagep::limits::kMaxFrameRate) ||
        !std::isfinite(frameCount) ||
        frameCount > static_cast<double>(imagep::limits::kMaxFrames) ||
        (frameCount > 0.0 &&
         frameCount / fps >
             static_cast<double>(imagep::limits::kMaxDurationSeconds))) {
        handle.release();
        return {Status::kInvalidArgument,
                "Video dimensions, frame count, or duration exceed limits"};
    }
    LOG(INFO) << "Video dimensions: " << width << "x" << height;
    return {Status::kOk, "Video read successfully"};
}

OpenCVImage::TinyStatus OpenCVImage::Video::procAndW(
    const Options* opt, const std::filesystem::path& dest,
    const ProcessingControl& control) {
    if (control.shouldStop()) {
        return {Status::kProcessingError,
                "Video processing cancelled or deadline exceeded"};
    }
    if (!handle.isOpened()) {
        return {Status::kInternalError, "No video data to rotate"};
    }

    // Get original video properties
    const auto frame_width =
        static_cast<int>(handle.get(cv::CAP_PROP_FRAME_WIDTH));
    const auto frame_height =
        static_cast<int>(handle.get(cv::CAP_PROP_FRAME_HEIGHT));
    const auto fps = handle.get(cv::CAP_PROP_FPS);

    int fourcc = 0;
    if (dest.extension() == ".mp4") {
        fourcc = cv::VideoWriter::fourcc('m', 'p', '4', 'v');
    } else if (dest.extension() == ".webm") {
        fourcc = cv::VideoWriter::fourcc('V', 'P', '8', '0');
    } else {
        return {Status::kInvalidArgument,
                "Unsupported output file format: " + dest.extension().string()};
    }

    const int angle = opt->rotate_angle.get();
    const bool swapsAxes = angle == kAngle90 || angle == kAngle270;
    const auto outputSize = swapsAxes ? cv::Size(frame_height, frame_width)
                                      : cv::Size(frame_width, frame_height);
    cv::VideoWriter writer(dest.string(), fourcc, fps, outputSize,
                           !opt->greyscale.get());

    if (!writer.isOpened()) {
        return {Status::kWriteError, "Failed to open the output video file: " +
                                         dest.filename().string()};
    }

    // Rotate each frame and write it to the output video
    cv::Mat frame;
    bool logOnce = false;
    std::uint64_t processedFrames = 0;
    const auto durationFrameLimit = std::max<std::uint64_t>(
        1, std::min<std::uint64_t>(
               imagep::limits::kMaxFrames,
               static_cast<std::uint64_t>(
                   fps * imagep::limits::kMaxDurationSeconds)));
    while (true) {
        if (control.shouldStop()) {
            writer.release();
            return {Status::kProcessingError,
                    "Video processing cancelled or deadline exceeded"};
        }
        handle >> frame;

        if (control.shouldStop()) {
            writer.release();
            return {Status::kProcessingError,
                    "Video processing cancelled or deadline exceeded"};
        }
        if (frame.empty()) {
            break;  // End of video
        }
        if (!imagep::limits::videoDimensionsAllowed(frame.cols, frame.rows)) {
            writer.release();
            return {Status::kInvalidArgument,
                    "Decoded video frame dimensions exceed limits"};
        }
        if (++processedFrames > durationFrameLimit) {
            writer.release();
            return {Status::kInvalidArgument,
                    "Video frame count exceeds processing limit"};
        }

        // Rotate the frame
        Image::rotate(frame, opt->rotate_angle.get());

        if (opt->greyscale.get()) {
            Image::greyscale(frame);
        }

        if (opt->invert_color.get()) {
            Image::invert(frame);
        }

        if (control.shouldStop()) {
            writer.release();
            return {Status::kProcessingError,
                    "Video processing cancelled or deadline exceeded"};
        }

        if (frame.cols != outputSize.width || frame.rows != outputSize.height) {
            cv::Mat resizedFrame;
            if (!logOnce) {
                LOG(INFO) << fmt::format(
                    "Converted frame: {}x{}, VideoWriter config: {}x{}, "
                    "resizing",
                    frame.cols, frame.rows, outputSize.width,
                    outputSize.height);
                logOnce = true;
            }
            cv::resize(frame, resizedFrame, outputSize);
            writer << resizedFrame;
        } else {
            // Write the rotated frame to the output video
            writer << frame;
        }
    }
    writer.release();
    if (control.shouldStop()) {
        return {Status::kProcessingError,
                "Video processing cancelled or deadline exceeded"};
    }
    std::error_code ec;
    const auto outputBytes = std::filesystem::file_size(dest, ec);
    if (ec || outputBytes > imagep::limits::kMaxOutputBytes) {
        return {Status::kWriteError,
                "Processed video output exceeds the size limit"};
    }
    return {Status::kOk, "Video processed and written successfully"};
}

OpenCVImage::TinyStatus OpenCVImage::Image::procAndW(
    const Options* opt, const std::filesystem::path& dest,
    const ProcessingControl& control) {
    if (control.shouldStop()) {
        return {Status::kProcessingError,
                "Image processing cancelled or deadline exceeded"};
    }
    if (handle.empty()) {
        return {Status::kInternalError, "No image data to process"};
    }

    // Convert the image to grayscale if required
    if (opt->greyscale.get()) {
        Image::greyscale(handle);
    }

    if (opt->invert_color.get()) {
        Image::invert(handle);
    }

    Image::rotate(handle, opt->rotate_angle.get());

    if (control.shouldStop()) {
        return {Status::kProcessingError,
                "Image processing cancelled or deadline exceeded"};
    }

    // Write the processed image to the output file
    try {
        if (!cv::imwrite(dest.string(), handle)) {
            return {Status::kWriteError,
                    "Failed to write image to: " + dest.string()};
        }
    } catch (const cv::Exception& ex) {
        return {Status::kWriteError, "Error writing image"};
    }
    if (control.shouldStop()) {
        return {Status::kProcessingError,
                "Image processing cancelled or deadline exceeded"};
    }
    return {Status::kOk, "Image processed and written successfully"};
}

OpenCVImage::TinyStatus OpenCVImage::processAndWrite(
    const std::filesystem::path& filename) {
    if (!component) {
        return {Status::kInternalError, "No image or video component loaded"};
    }
    return component->procAndW(&options, filename, processingControl);
}

std::string OpenCVImage::version() const {
    return "OpenCV version: " + cv::getVersionString();
}
