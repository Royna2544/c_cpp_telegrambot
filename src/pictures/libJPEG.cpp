// clang-format off
#include <csetjmp>
#include <cstddef>
#include <cstdio>
#include <jpeglib.h>
// clang-format on
#include <absl/log/check.h>
#include <absl/log/log.h>

#include <cstring>
#include <libJPEG.hpp>
#include <memory>

struct jpegimg_error_mgr {
    jpeg_error_mgr pub;
    jmp_buf setjmpbuf;
};
using jpegimg_error_ptr = jpegimg_error_mgr*;

METHODDEF(void)
jpegimg_error_exit(j_common_ptr cinfo) {
    const auto myerr = (jpegimg_error_ptr)cinfo->err;
    (*cinfo->err->output_message)(cinfo);
    longjmp(myerr->setjmpbuf, 1);
}

bool JPEGImage::read(const std::filesystem::path& filename) {
    FILE* infile = fopen(filename.string().c_str(), "rb");
    if (infile == nullptr) {
        LOG(ERROR) << "Error opening file: " << filename;
        return false;
    }
    const auto autoCloser = createFileCloser(infile);

    LOG(INFO) << "Loading image " << filename;

    jpeg_decompress_struct cinfo{};
    jpegimg_error_mgr jerr{};

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpegimg_error_exit;

    if (setjmp(jerr.setjmpbuf)) {
        LOG(ERROR) << "Error creating decompress struct";
        return false;
    }

    jpeg_create_decompress(&cinfo);

    if (setjmp(jerr.setjmpbuf)) {
        LOG(ERROR) << "Error decompressing JPEG file";
        jpeg_destroy_decompress(&cinfo);
        return false;
    }

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

    return true;
}

JPEGImage::Result JPEGImage::_rotate_image(int angle) {
    size_t new_width = 0;
    size_t new_height = 0;
    std::unique_ptr<unsigned char[]> new_image_data = nullptr;

    switch (angle) {
        case kAngle90:
        case kAngle270:
            new_width = height;
            new_height = width;
            break;
        case kAngle180:
            new_width = width;
            new_height = height;
            break;
        default:
            LOG(WARNING) << "libJPEG cannot handle angle: " << angle;
            return Result::kErrorUnsupportedAngle;
    }

    new_image_data = std::make_unique<unsigned char[]>(new_width * new_height *
                                                       num_channels);
    for (size_t y = 0; y < height; ++y) {
        for (size_t x = 0; x < width; ++x) {
            for (int c = 0; c < num_channels; ++c) {
                switch (angle) {
                    case kAngle90:
                        new_image_data[(x * new_width + (new_width - y - 1)) *
                                           num_channels +
                                       c] =
                            image_data[(y * width + x) * num_channels + c];
                        break;
                    case kAngle180:
                        new_image_data[((new_height - y - 1) * new_width +
                                        (new_width - x - 1)) *
                                           num_channels +
                                       c] =
                            image_data[(y * width + x) * num_channels + c];
                        break;
                    case kAngle270:
                        new_image_data[((new_height - x - 1) * new_width + y) *
                                           num_channels +
                                       c] =
                            image_data[(y * width + x) * num_channels + c];
                        break;
                    default:
                        CHECK(false);
                }
            }
        }
    }

    image_data = std::move(new_image_data);
    width = new_width;
    height = new_height;
    return Result::kSuccess;
}

void JPEGImage::to_greyscale() {
    if (num_channels < 3) {
        LOG(WARNING)
            << "Image does not have enough color channels to convert to "
               "grayscale.";
        return;
    }

    for (size_t i = 0; i < width * height * num_channels; i += num_channels) {
        const auto grey = static_cast<unsigned char>(0.299 * image_data[i] +
                                                     0.587 * image_data[i + 1] +
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