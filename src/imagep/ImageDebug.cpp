#include "ImageDebug.hpp"

#include <absl/log/log.h>

#include <filesystem>

DebugImage::TinyStatus DebugImage::read(const std::filesystem::path& filename,
                                        Target /*target*/) {
    LOG(WARNING) << "Entering DebugImage::read";

    auto newfile = "DebugImageCopy_" + filename.filename().string();
    LOG(INFO) << "I will copy the file to " << newfile;
    std::error_code ec;
    std::filesystem::path outputPath =
        std::filesystem::temp_directory_path(ec) / newfile;
    if (ec) {
        LOG(ERROR) << "Failed to get temporary directory: " << ec.message();
        return {DebugImage::Status::kInternalError,
                "Temporary directory retrieval failed"};
    }
    std::filesystem::copy_file(
        filename, outputPath, std::filesystem::copy_options::overwrite_existing,
        ec);
    if (ec) {
        LOG(ERROR) << "Failed to copy file to " << outputPath << ": "
                   << ec.message();
        return {DebugImage::Status::kInternalError, "File copy failed"};
    }
    LOG(INFO) << "File copied to " << outputPath;

    return DebugImage::TinyStatus::ok();
}

DebugImage::TinyStatus DebugImage::processAndWrite(
    const std::filesystem::path& /*filename*/) {
    LOG(INFO) << "DebugImage::processAndWrite: Noop";
    return DebugImage::TinyStatus::ok();
}
