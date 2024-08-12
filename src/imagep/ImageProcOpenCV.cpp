#include "ImageProcOpenCV.hpp"

#include <opencv2/core/utility.hpp>

#include "absl/status/status.h"

bool OpenCVImage::read(const std::filesystem::path& filename) {
    image = cv::imread(filename.string(), cv::IMREAD_UNCHANGED);
    if (image.empty()) {
        LOG(ERROR) << "Error reading image: " << filename;
        return false;
    }
    return true;
}

absl::Status OpenCVImage::_rotate_image(int angle) {
    // Make it clockwise
    angle = kAngleMax - angle;

    cv::Point2d center(image.cols / 2.0, image.rows / 2.0);
    cv::Mat rotation_matrix = cv::getRotationMatrix2D(center, angle, 1.0);

    // Calculate the bounding box of the rotated image
    cv::Rect bbox =
        cv::RotatedRect(cv::Point2f(), image.size(), angle).boundingRect();

    // Adjust the rotation matrix to take into account translation
    rotation_matrix.at<double>(0, 2) += bbox.width / 2.0 - center.x;
    rotation_matrix.at<double>(1, 2) += bbox.height / 2.0 - center.y;

    cv::Mat rotated_image;
    cv::warpAffine(image, rotated_image, rotation_matrix, bbox.size());

    image = rotated_image;
    return absl::OkStatus();
}

void OpenCVImage::to_greyscale() {
    if (image.channels() == 3) {
        // Convert a BGR image to grayscale
        cv::Mat gray_image;
        cv::cvtColor(image, gray_image, cv::COLOR_BGR2GRAY);
        gray_image.copyTo(image);  // Update the image with the grayscale data
    } else if (image.channels() == 4) {
        // Convert a BGRA image to grayscale while preserving the alpha channel
        cv::Mat gray_image;
        cv::Mat alpha_channel;
        std::vector<cv::Mat> channels(4);

        // Split the channels
        cv::split(image, channels);
        cv::cvtColor(image, gray_image, cv::COLOR_BGRA2GRAY);

        // Extract the alpha channel
        alpha_channel = channels[3];

        // Merge grayscale and alpha channel back into one image
        cv::Mat gray_alpha_image;
        std::vector<cv::Mat> gray_alpha_channels = {gray_image, gray_image,
                                                    gray_image, alpha_channel};
        cv::merge(gray_alpha_channels, gray_alpha_image);

        gray_alpha_image.copyTo(image);  // Update the image with the grayscale
                                         // data and preserved alpha channel
    } else {
        LOG(INFO) << "Image does not have enough color channels to convert to "
                     "grayscale.";
    }
}

bool OpenCVImage::write(const std::filesystem::path& filename) {
    if (!cv::imwrite(filename.string(), image)) {
        LOG(INFO) << "Error writing image: " << filename;
        return false;
    }
    return true;
}

std::string OpenCVImage::version() const {
    return "OpenCV version: " + cv::getVersionString();
}