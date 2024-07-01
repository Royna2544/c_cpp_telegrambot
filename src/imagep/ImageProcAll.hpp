#pragma once

#include <memory>

#ifdef HAVE_OPENCV
#include "ImageProcOpenCV.hpp"
#endif
#ifdef HAVE_LIBJPEG
#include "ImageTypeJPEG.hpp"
#endif
#ifdef HAVE_LIBPNG
#include "ImageTypePNG.hpp"
#endif
#ifdef HAVE_LIBWEBP
#include "ImageTypeWEBP.hpp"
#endif
#include "imagep/ImagePBase.hpp"

struct ImageProcessingAll {
    bool read();
    PhotoBase::Result rotate(int angle);
    void to_greyscale();
    bool write(const std::filesystem::path& filename);

    explicit ImageProcessingAll(std::filesystem::path filename);

   private:
    std::filesystem::path _filename;
    std::vector<std::unique_ptr<PhotoBase>> impls;
    std::unique_ptr<PhotoBase> _impl;
};