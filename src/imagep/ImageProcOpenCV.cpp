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

    cv::Point2f center(image.cols / 2.0, image.rows / 2.0);
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
        cv::cvtColor(image, image, cv::COLOR_BGR2GRAY);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, image, cv::COLOR_BGRA2GRAY);
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