#include "ImageTypePNG.hpp"

#include <absl/log/log.h>
#include <absl/status/status.h>
#include <png.h>
#include <pngconf.h>

#include <StructF.hpp>
#include <cstddef>
#include <filesystem>
#include <string>

#if (PNG_LIBPNG_VER < 10500)
#define png_longjmp_fn(png, val) longjmp(png->jmpbuf, val);
#else
#define png_longjmp_fn(png, val) png_longjmp(png, val);
#endif

namespace {
void absl_warn_fn(png_structp png_ptr, png_const_charp error_message) {
    LOG(WARNING) << "libpng: " << error_message;
    png_longjmp_fn(png_ptr, 1);
}

void absl_error_fn(png_structp png_ptr, png_const_charp error_message) {
    LOG(ERROR) << "libpng: " << error_message;
    png_longjmp_fn(png_ptr, 1);
}
}  // namespace

absl::Status PngImage::read(const std::filesystem::path& filename,
                            const Target target) {
    if (target != Target::kPhoto) {
        return absl::InvalidArgumentError("Invalid target for PNG image");
    }

    F fp;
    png_structp png = nullptr;
    png_infop info = nullptr;

    if (contains_data) {
        return absl::InvalidArgumentError("Already contains data");
    }

    if (!fp.open(filename, F::Mode::ReadBinary)) {
        return absl::InternalError("Can't open file for reading");
    }

    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr,
                                 nullptr);
    if (png == nullptr) {
        return absl::InternalError("png_create_read_struct failed");
    }

    info = png_create_info_struct(png);
    if (info == nullptr) {
        return absl::InternalError("png_create_info_struct failed");
    }

    png_set_error_fn(png, nullptr, absl_error_fn, absl_warn_fn);

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        return absl::InternalError("Error during reading image header");
    }

    png_init_io(png, fp);
    png_read_info(png, info);

    width = png_get_image_width(png, info);
    height = png_get_image_height(png, info);
    color_type = png_get_color_type(png, info);
    bit_depth = png_get_bit_depth(png, info);

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        return absl::InternalError("Error during updating image information");
    }
    if (bit_depth == 16) {
        png_set_strip_16(png);
    }
    if (color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }

    if (png_get_valid(png, info, PNG_INFO_tRNS) != 0U) {
        png_set_tRNS_to_alpha(png);
    }
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_PALETTE) {
        png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    }
    if (color_type == PNG_COLOR_TYPE_GRAY ||
        color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }
    png_read_update_info(png, info);

    for (int y = 0; y < height; y++) {
        refmem.add(png_get_rowbytes(png, info));
    }

    png_read_image(png, refmem.data());

    png_destroy_read_struct(&png, &info, nullptr);

    contains_data = true;
    return absl::OkStatus();
}

void PngImage::greyscale() {
    if (!contains_data) {
        LOG(ERROR) << "No image data to convert to greyscale";
        return;
    }
    LOG(INFO) << "Converting image to greyscale";
    for (ptrdiff_t y = 0; y < height; y++) {
        png_bytep row = refmem[y];
        for (ptrdiff_t x = 0; x < width; x++) {
            png_bytep px = &(row[x * 4]);
            const auto gray = static_cast<uint8_t>(
                0.299 * px[0] + 0.587 * px[1] + 0.114 * px[2]);
            px[0] = px[1] = px[2] = gray;
        }
    }
}

void PngImage::rotate_image_impl(png_uint_32 new_width, png_uint_32 new_height,
                                 const transform_fn_t& transform) {
    PngImage src = *this;

    width = new_width;
    height = new_height;
    color_type = src.color_type;
    bit_depth = src.bit_depth;

    for (int y = 0; y < height; y++) {
        refmem.add(static_cast<size_t>(width) * 4);
    }

    for (ptrdiff_t y = 0; y < src.height; y++) {
        for (ptrdiff_t x = 0; x < src.width; x++) {
            ptrdiff_t dst_x = 0;
            ptrdiff_t dst_y = 0;
            transform(src.width, src.height, dst_x, dst_y, x, y);
            png_bytep src_pixel = &src.refmem[y][x * 4];
            png_bytep dst_pixel = &refmem[dst_y][dst_x * 4];
            memcpy(dst_pixel, src_pixel, 4);
        }
    }
}

absl::Status PngImage::rotate(int angle) {
    if (!contains_data) {
        return absl::UnavailableError("No image data to rotate");
    }

    switch (angle) {
        case kAngle90:
            rotate_image_impl(
                height, width,
                [](png_uint_32 src_width, png_uint_32 src_height,
                   ptrdiff_t& dst_x, ptrdiff_t& dst_y, int x, int y) {
                    dst_x = src_height - 1 - y;
                    dst_y = x;
                });
            break;
        case kAngle180:
            rotate_image_impl(
                width, height,
                [](png_uint_32 src_width, png_uint_32 src_height,
                   ptrdiff_t& dst_x, ptrdiff_t& dst_y, int x, int y) {
                    dst_x = src_width - 1 - x;
                    dst_y = src_height - 1 - y;
                });
            break;
        case kAngle270:
            rotate_image_impl(
                height, width,
                [](png_uint_32 src_width, png_uint_32 src_height,
                   ptrdiff_t& dst_x, ptrdiff_t& dst_y, int x, int y) {
                    dst_x = y;
                    dst_y = src_width - 1 - x;
                });
            break;
        case kAngleMin:
            // Noop
            return absl::OkStatus();
        default:
            LOG(WARNING) << "libPNG cannot handle angle: " << angle;
            return absl::UnimplementedError("Cannot handle angle");
    }
    LOG(INFO) << "New dimensions: " << width << "x" << height;
    return absl::OkStatus();
}

void PngImage::invert() {
    if (!contains_data) {
        LOG(ERROR) << "No image data to invert";
        return;
    }
    LOG(INFO) << "Inverting image";
    for (ptrdiff_t y = 0; y < height; y++) {
        png_bytep row = refmem[y];
        for (ptrdiff_t x = 0; x < width; x++) {
            png_bytep px = &(row[x * 4]);
            px[0] = ~px[0];
            px[1] = ~px[1];
            px[2] = ~px[2];
        }
    }
}

absl::Status PngImage::processAndWrite(const std::filesystem::path& filename) {
    F fp;
    png_structp png = nullptr;
    png_infop info = nullptr;

    if (!contains_data) {
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

    if (!fp.open(filename, F::Mode::WriteBinary)) {
        return absl::InternalError("Can't open file for writing: " + filename.string());
    }

    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr,
                                  nullptr);
    if (png == nullptr) {
        return absl::InternalError("png_create_write_struct failed");
    }

    info = png_create_info_struct(png);
    if (info == nullptr) {
        return absl::InternalError("png_create_info_struct failed");
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        return absl::InternalError("Error during init_io");
    }

    png_init_io(png, fp);

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        return absl::InternalError("Error during writing header");
    }

    png_set_IHDR(png, info, width, height, bit_depth, color_type,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT,
                 PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        return absl::InternalError("Error during writing bytes");
    }

    png_write_image(png, refmem.data());

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        return absl::InternalError("Error during end of write");
    }

    png_write_end(png, nullptr);

    png_destroy_write_struct(&png, &info);

    contains_data = false;
    return absl::OkStatus();
}

std::string PngImage::version() const { return "libPNG version " PNG_LIBPNG_VER_STRING; }
