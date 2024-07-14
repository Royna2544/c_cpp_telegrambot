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
#include "ImagePBase.hpp"

#include <TgBotImgProcExports.h>

struct TgBotImgProc_API ImageProcessingAll {
    bool read();
    absl::Status rotate(int angle);
    void to_greyscale();
    bool write(const std::filesystem::path& filename);

    explicit ImageProcessingAll(std::filesystem::path filename);

   private:
    std::filesystem::path _filename;
    std::vector<std::unique_ptr<PhotoBase>> impls;
    std::unique_ptr<PhotoBase> _impl;
};