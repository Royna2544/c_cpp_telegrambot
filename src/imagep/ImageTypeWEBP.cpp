#include "ImageTypeWEBP.hpp"

#include <AbslLogCompat.hpp>
#include <AbslLogCompat.hpp>
#include <absl/status/status.h>
#include <webp/decode.h>
#include <webp/encode.h>
#include <webp/types.h>

#include <StructF.hpp>
#include <cstddef>
#include <cstdint>
#include <memory>

absl::Status WebPImage::read(const std::filesystem::path& filename,
                             const Target target) {
    int width = 0;
    int height = 0;
    uint8_t* decoded_data = nullptr;
    F file;

    if (target != Target::kPhoto) {
        return absl::InvalidArgumentError("Invalid target for WebP image");
    }

    // Open the file for reading in binary mode
    if (!file.open(filename, F::Mode::ReadBinary)) {
        return absl::InternalError("Can't open file for reading");
    }

    const auto file_size = file.size();

    auto data = std::make_unique_for_overwrite<uint8_t[]>(file_size);
    if (!file.read(data.get(), 1, file_size)) {
        return absl::InternalError("Failed to read from file");
    }
    file.close();

    decoded_data = WebPDecodeRGBA(data.get(), file_size, &width, &height);

    if (decoded_data == nullptr) {
        return absl::InternalError("Couldn't decode image data");
    }

    width_ = width;
    height_ = height;
    const auto bufferSize = width * height * 4;
    data_ = std::make_unique_for_overwrite<uint8_t[]>(bufferSize);
    memcpy(data_.get(), decoded_data, bufferSize);
    WebPFree(decoded_data);

    return absl::OkStatus();
}

absl::Status WebPImage::rotate(int angle) {
    if (data_ == nullptr) {
        return absl::NotFoundError("No image data to rotate");
    }

    webpimage_size_t rotated_width = 0;
    webpimage_size_t rotated_height = 0;
    std::unique_ptr<uint8_t[]> rotated_data = nullptr;

    switch (angle) {
        case kAngle90:
        case kAngle270:
            rotated_width = height_;
            rotated_height = width_;
            break;
        case kAngle180:
            rotated_width = width_;
            rotated_height = height_;
            break;
        case kAngleMin:
            // noop
            return absl::OkStatus();
        default:
            LOG(WARNING) << "libWEBP cannot handle angle: " << angle;
            return absl::UnimplementedError("Cannot handle angle");
    }

    const size_t rotated_width_long = rotated_width;
    rotated_data =
        std::make_unique<uint8_t[]>(rotated_width_long * rotated_height * 4);

    for (int y = 0; y < height_; ++y) {
        for (int x = 0; x < width_; ++x) {
            const auto source_index = (y * width_ + x) * 4;
            int dest_index = 0;

            switch (angle) {
                case kAngle90:
                    dest_index = (x * rotated_width + (height_ - 1 - y)) * 4;
                    break;
                case kAngle180:
                    dest_index =
                        ((height_ - 1 - y) * width_ + (width_ - 1 - x)) * 4;
                    break;
                case kAngle270:
                    dest_index = ((width_ - 1 - x) * rotated_width + y) * 4;
                    break;
                default:
                    CHECK(false);
            }
            for (int k = 0; k < 4; ++k) {
                rotated_data[dest_index + k] = data_[source_index + k];
            }
        }
    }

    data_ = std::move(rotated_data);
    width_ = rotated_width;
    height_ = rotated_height;
    return absl::OkStatus();
}

void WebPImage::greyscale() {
    if (data_ == nullptr) {
        LOG(ERROR) << "No image data to convert to greyscale";
        return;
    }

    for (size_t i = 0; i < static_cast<size_t>(width_) * height_; ++i) {
        // Convert each pixel to greyscale by averaging RGB values
        uint8_t grey = (data_[i * 4] + data_[i * 4 + 1] + data_[i * 4 + 2]) / 3;
        data_[i * 4] = grey;
        data_[i * 4 + 1] = grey;
        data_[i * 4 + 2] = grey;
    }
}


void WebPImage::invert() {
    if (data_ == nullptr) {
        LOG(ERROR) << "No image data to invert";
        return;
    }

    for (size_t i = 0; i < static_cast<size_t>(width_) * height_; ++i) {
        data_[i * 4] = std::numeric_limits<uint8_t>::max() - data_[i * 4];
        data_[i * 4 + 1] = std::numeric_limits<uint8_t>::max() - data_[i * 4 + 1];
        data_[i * 4 + 2] = std::numeric_limits<uint8_t>::max() - data_[i * 4 + 2];
    }
}

absl::Status WebPImage::processAndWrite(const std::filesystem::path& filename) {
    if (data_ == nullptr) {
        return absl::NotFoundError("No image data to write");
    }

    if (options.greyscale.get()) {
        greyscale();
    }
    auto err = rotate(options.rotate_angle.get());
    if (!err.ok()) {
        return err;
    }
    if (options.invert_color.get()) {
        invert();
    }

    F file;
    if (!file.open(filename, F::Mode::WriteBinary)) {
        return absl::InternalError("Can't open file: " + filename.string());
    }

    int stride = width_ * 4;
    uint8_t* output = nullptr;
    size_t output_size = 0;

    output_size =
        WebPEncodeRGBA(data_.get(), width_, height_, stride, QUALITY, &output);
    if (output_size == 0) {
        return absl::InternalError("Failed to encode WebP image");
    }

    if (!file.write(output, 1, output_size)) {
        return absl::InternalError("Failed to write to file");
    }
    file.close();
    WebPFree(output);

    LOG(INFO) << "New WebP image written to " << filename;
    return absl::OkStatus();
}

std::string WebPImage::version() const {
    constexpr int kVersionBits = 8;
    constexpr int kVersionMask = 0xff;
    int version = WebPGetDecoderVersion();
    int major = (version >> kVersionBits >> kVersionBits) & kVersionMask;
    int minor = (version >> kVersionBits) & kVersionMask;
    int revision = version & kVersionMask;

    return ("libwebp version: " + std::to_string(major) + "." +
            std::to_string(minor) + "." + std::to_string(revision));
}