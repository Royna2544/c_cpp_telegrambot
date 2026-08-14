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
#ifdef IMAGEPROC_HAVE_LIBJPEG
    impls.emplace_back(std::make_unique<JPEGImage>());
#endif
#ifdef IMAGEPROC_HAVE_LIBPNG
    impls.emplace_back(std::make_unique<PngImage>());
#endif
#ifdef IMAGEPROC_HAVE_LIBWEBP
    impls.emplace_back(std::make_unique<WebPImage>());
#endif
}

bool ImageProcessingAll::read(PhotoBase::Target target, std::stop_token stop,
                              std::chrono::steady_clock::time_point deadline) {
    const PhotoBase::ProcessingControl control{stop, deadline};
    if (control.shouldStop()) {
        return false;
    }
    LOG(INFO) << "read(): file=" << _filename << " target=" << target;
#ifdef IMAGEPROC_HAVE_OPENCV
    if (target == PhotoBase::Target::kVideo) {
        auto video = std::make_unique<OpenCVImage>();
        video->processingControl = control;
        const auto result = video->read(_filename, target);
        if (result.isOk() && !control.shouldStop()) {
            _impl = std::move(video);
            impls.clear();
            return true;
        }
        LOG(INFO) << "Failed to read video: " << result;
        return false;
    }
#endif
    for (auto& impl : impls) {
        if (control.shouldStop()) {
            return false;
        }
        impl->processingControl = control;
        DLOG(INFO) << "Trying to read with impl: " << impl->version();
        auto ret = impl->read(_filename, target);
        if (ret.isOk() && !control.shouldStop()) {
            // We found the backend suitable. Select one and dealloc others.
            _impl = std::move(impl);
            impls.clear();

            LOG(INFO) << "Successfully read with backend: " << _impl->version();
            return true;
        }
        LOG(INFO) << "Failed to read: " << ret;
    }
    LOG(INFO) << "No backend was suitable to read";
    return false;
}

DebugImage::TinyStatus ImageProcessingAll::processAndWrite(
    const std::filesystem::path& filename, std::stop_token stop,
    std::chrono::steady_clock::time_point deadline) {
    if (!_impl) {
        LOG(ERROR) << "No backend selected for writing";
        return {DebugImage::Status::kInternalError,
                "No backend selected for writing"};
    }
    _impl->processingControl = {stop, deadline};
    if (_impl->processingControl.shouldStop()) {
        return {DebugImage::Status::kProcessingError,
                "Media processing cancelled or deadline exceeded"};
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
