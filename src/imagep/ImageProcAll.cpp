#include "ImageProcAll.hpp"

#include <filesystem>

#include "imagep/ImagePBase.hpp"

ImageProcessingAll::ImageProcessingAll(std::filesystem::path filename)
    : _filename(std::move(filename)) {
#ifdef HAVE_OPENCV
    impls.emplace_back(std::make_unique<OpenCVImage>());
#endif
#ifdef HAVE_LIBJPEG
    impls.emplace_back(std::make_unique<JPEGImage>());
#endif
#ifdef HAVE_LIBPNG
    impls.emplace_back(std::make_unique<PngImage>());
#endif
#ifdef HAVE_LIBWEBP
    impls.emplace_back(std::make_unique<WebPImage>());
#endif
}

bool ImageProcessingAll::read() {
    for (auto& impl : impls) {
        if (impl->read(_filename)) {
            // We found the backend suitable. Select one and dealloc others.
            _impl = std::move(impl);
            impls.clear();

            LOG(INFO) << "Using implementation: " << _impl->version();
            return true;
        }
    }
    LOG(INFO) << "No backend was suitable to read";
    return false;
}

PhotoBase::Result ImageProcessingAll::rotate(int angle) {
    if (!_impl) {
        LOG(ERROR) << "No backend selected for rotation";
        return PhotoBase::Result::kErrorNoData;
    }
    DLOG(INFO) << "Calling impl->rotate with angle: " << angle;
    return _impl->rotate_image(angle);
}

void ImageProcessingAll::to_greyscale() {
    if (!_impl) {
        LOG(ERROR) << "No backend selected for greyscale conversion";
        return;
    }
    DLOG(INFO) << "Calling impl->to_greyscale";
    _impl->to_greyscale();
}

bool ImageProcessingAll::write(const std::filesystem::path& filename) {
    if (!_impl) {
        LOG(ERROR) << "No backend selected for writing";
        return false;
    }
    if (filename.empty()) {
        LOG(ERROR) << "Filename is empty";
        return false;
    }
    DLOG(INFO) << "Calling impl->write with filename: " << filename;
    return _impl->write(filename);
}