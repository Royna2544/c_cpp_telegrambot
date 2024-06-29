// clang-format off
#include <cstddef>
#include <cstdio>
#include <jpeglib.h>
// clang-format on
#include <absl/log/log.h>

#include <cstring>
#include <libJPEG.hpp>
#include <memory>

bool JPEGImage::read(const std::filesystem::path& filename) {
    FILE* infile = fopen(filename.string().c_str(), "rb");
    if (infile == nullptr) {
        LOG(ERROR) << "Error opening file: " << filename;
        return false;
    }

    LOG(INFO) << "Loading image " << filename;
    
    jpeg_decompress_struct cinfo{};
    jpeg_error_mgr jerr{};

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    width = cinfo.output_width;
    height = cinfo.output_height;
    num_channels = cinfo.output_components;

    size_t row_stride = width * num_channels;
    image_data =
        std::make_unique<unsigned char[]>(width * height * num_channels);
    std::array<unsigned char*, 1> rowptr{};

    while (cinfo.output_scanline < height) {
        rowptr[0] = &image_data[(cinfo.output_scanline) * row_stride];
        jpeg_read_scanlines(&cinfo, rowptr.data(), 1);
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);

    return true;
}

void JPEGImage::rotate_image_90() { rotate_image(90); }

void JPEGImage::rotate_image_180() { rotate_image(180); }

void JPEGImage::rotate_image_270() { rotate_image(270); }

void JPEGImage::rotate_image(int angle) {
    size_t new_width = 0;
    size_t new_height = 0;
    std::unique_ptr<unsigned char[]> new_image_data = nullptr;

    if (angle == 90 || angle == 270) {
        new_width = height;
        new_height = width;
        new_image_data = std::make_unique<unsigned char[]>(
            new_width * new_height * num_channels);

        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                for (int c = 0; c < num_channels; ++c) {
                    if (angle == 90) {
                        new_image_data[(x * new_width + (new_width - y - 1)) *
                                           num_channels +
                                       c] =
                            image_data[(y * width + x) * num_channels + c];
                    } else if (angle == 270) {
                        new_image_data[((new_height - x - 1) * new_width + y) *
                                           num_channels +
                                       c] =
                            image_data[(y * width + x) * num_channels + c];
                    }
                }
            }
        }
    } else if (angle == 180) {
        new_width = width;
        new_height = height;
        new_image_data = std::make_unique<unsigned char[]>(
            new_width * new_height * num_channels);

        for (size_t y = 0; y < height; ++y) {
            for (size_t x = 0; x < width; ++x) {
                for (int c = 0; c < num_channels; ++c) {
                    new_image_data[((new_height - y - 1) * new_width +
                                    (new_width - x - 1)) *
                                       num_channels +
                                   c] =
                        image_data[(y * width + x) * num_channels + c];
                }
            }
        }
    } else {
        LOG(ERROR) << "Unsupported rotation angle: " << angle << " degrees.";
        return;
    }

    image_data = std::move(new_image_data);
    width = new_width;
    height = new_height;
}

void JPEGImage::to_greyscale() {
    if (num_channels < 3) {
        LOG(WARNING)
            << "Image does not have enough color channels to convert to "
               "grayscale.";
        return;
    }

    for (size_t i = 0; i < width * height * num_channels; i += num_channels) {
        unsigned char grey = static_cast<unsigned char>(
            0.299 * image_data[i] + 0.587 * image_data[i + 1] +
            0.114 * image_data[i + 2]);
        image_data[i] = image_data[i + 1] = image_data[i + 2] = grey;
    }
}

bool JPEGImage::write(const std::filesystem::path& filename) {
    FILE* outfile = fopen(filename.string().c_str(), "wb");
    if (outfile == nullptr) {
        LOG(ERROR) << "Error opening file: " << filename;
        return false;
    }

    jpeg_compress_struct cinfo{};
    jpeg_error_mgr jerr{};

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, outfile);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = num_channels;
    cinfo.in_color_space = num_channels == 3 ? JCS_RGB : JCS_GRAYSCALE;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 95, TRUE);
    jpeg_start_compress(&cinfo, TRUE);

    size_t row_stride = width * num_channels;
    std::array<unsigned char*, 1> rowptr{};

    while (cinfo.next_scanline < height) {
        rowptr[0] = &image_data[cinfo.next_scanline * row_stride];
        jpeg_write_scanlines(&cinfo, rowptr.data(), 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(outfile);

    return true;
}