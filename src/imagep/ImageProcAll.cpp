#include "ImageProcAll.hpp"

#include <filesystem>

#ifdef IMAGEPROC_HAVE_OPENCV
#include "ImageProcOpenCV.hpp"
#endif
#ifdef IMAGEPROC_HAVE_LIBJPEG
#include "ImageTypeJPEG.hpp"
#endif
#ifdef IMAGEPROC_HAVE_LIBPNG
#include "ImageTypePNG.hpp"
#endif
#ifdef IMAGEPROC_HAVE_LIBWEBP
#include "ImageTypeWEBP.hpp"
#endif
#include "ImageDebug.hpp"

ImageProcessingAll::ImageProcessingAll(std::filesystem::path filename)
    : _filename(std::move(filename)) {
#ifdef IMAGEPROC_HAVE_OPENCV
    impls.emplace_back(std::make_unique<OpenCVImage>());
#endif
#ifdef IMAGEPROC_HAVE_LIBJPEG
    impls.emplace_back(std::make_unique<JPEGImage>());
#endif
#ifdef IMAGEPROC_HAVE_LIBPNG
    impls.emplace_back(std::make_unique<PngImage>());
#endif
#ifdef IMAGEPROC_HAVE_LIBWEBP
    impls.emplace_back(std::make_unique<WebPImage>());
#endif
#ifndef NDEBUG
    impls.emplace_back(std::make_unique<DebugImage>());
#endif
}

bool ImageProcessingAll::read(PhotoBase::Target target) {
    LOG(INFO) << "read(): file=" << _filename << " target=" << target;
    for (auto& impl : impls) {
        DLOG(INFO) << "Trying to read with impl: " << impl->version();
        auto ret = impl->read(_filename, target);
        if (ret.isOk()) {
            // We found the backend suitable. Select one and dealloc others.
            _impl = std::move(impl);
            impls.clear();

            LOG(INFO) << "Successfully read";
            return true;
        }
        DLOG(INFO) << "Failed to read: " << ret;
    }
    LOG(INFO) << "No backend was suitable to read";
    return false;
}

DebugImage::TinyStatus ImageProcessingAll::processAndWrite(
    const std::filesystem::path& filename) {
    if (!_impl) {
        LOG(ERROR) << "No backend selected for writing";
        return {DebugImage::Status::kInternalError,
                "No backend selected for writing"};
    }
    DLOG(INFO) << "Passing options to backend";
    _impl->options = options;
    DLOG(INFO) << "Calling impl->processAndWrite with filename: " << filename;
    auto ret = _impl->processAndWrite(filename);
    if (!ret.isOk()) {
        LOG(ERROR) << "impl->processAndWrite returned: " << ret;
    }
    return ret;
}