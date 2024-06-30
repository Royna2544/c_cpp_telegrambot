#include "libOpenCV.hpp"

bool OpenCVImage::read(const std::filesystem::path& filename) {
    image = cv::imread(filename.string(), cv::IMREAD_UNCHANGED);
    if (image.empty()) {
        LOG(ERROR) << "Error reading image: " << filename;
        return false;
    }
    LOG(INFO) << "Loading image: " << filename << ": Success";
    return true;
}

OpenCVImage::Result OpenCVImage::_rotate_image(int angle) {
    LOG(INFO) << "Rotating image";

    cv::Point2f center(image.cols / 2.0, image.rows / 2.0);
    cv::Mat rotation_matrix = cv::getRotationMatrix2D(center, angle, 1.0);

    // Calculate the bounding box of the rotated image
    cv::Rect bbox = cv::RotatedRect(cv::Point2f(), image.size(), angle).boundingRect();

    // Adjust the rotation matrix to take into account translation
    rotation_matrix.at<double>(0, 2) += bbox.width / 2.0 - center.x;
    rotation_matrix.at<double>(1, 2) += bbox.height / 2.0 - center.y;

    cv::Mat rotated_image;
    cv::warpAffine(image, rotated_image, rotation_matrix, bbox.size());

    image = rotated_image;
    return Result::kSuccess;
}

void OpenCVImage::to_greyscale() {
    if (image.channels() == 3) {
        cv::cvtColor(image, image, cv::COLOR_BGR2GRAY);
    } else if (image.channels() == 4) {
        cv::cvtColor(image, image, cv::COLOR_BGRA2GRAY);
    } else {
        std::cerr << "Image does not have enough color channels to convert to grayscale." << std::endl;
    }
}

bool OpenCVImage::write(const std::filesystem::path& filename) {
    if (!cv::imwrite(filename.string(), image)) {
        std::cerr << "Error writing image: " << filename << std::endl;
        return false;
    }
    return true;
}
