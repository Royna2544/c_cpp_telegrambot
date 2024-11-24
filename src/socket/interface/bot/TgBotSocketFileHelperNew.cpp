#include "TgBotSocketFileHelperNew.hpp"

#include <absl/log/log.h>

#ifdef __TGBOT__
#include <StructF.hpp>
#else
#include "../../../include/StructF.hpp"
#endif

#include "TgBotSocket_Export.hpp"

bool RealFS::writeFile(const std::filesystem::path& filename,
                       const uint8_t* startAddr, const size_t size) {
    size_t writtenCnt = 0;
    F file;

    LOG(INFO) << "Writing to file " << filename << "...";
    if (!file.open(filename, F::Mode::WriteBinary)) {
        LOG(ERROR) << "Failed to open file";
        return false;
    }
    if (!file.write(startAddr, size, 1)) {
        LOG(ERROR) << "Failed to write to file";
        return false;
    }
    LOG(INFO) << "Wrote " << size << " bytes to file " << filename;
    return true;
}

std::optional<SharedMalloc> RealFS::readFile(
    const std::filesystem::path& filename) {
    std::error_code errc;
    F file;

    if (!file.open(filename, F::Mode::ReadBinary)) {
        LOG(ERROR) << "Failed to fopen file: " << filename;
        return std::nullopt;
    }

    const auto file_size = std::filesystem::file_size(filename, errc);
    if (errc) {
        LOG(ERROR) << "Failed to get file size: " << filename << ": "
                   << errc.message();
        return std::nullopt;
    }

    SharedMalloc dataP(file_size);
    if (!file.read(dataP.get(), file_size, 1)) {
        LOG(ERROR) << "Failed to read from file";
        return std::nullopt;
    }
    return std::move(dataP);
}

bool RealFS::exists(const std::filesystem::path& path) {
    std::error_code errc;
    return std::filesystem::exists(path, errc);
}

void RealFS::SHA256(const SharedMalloc& memory, HashContainer& data) {
    ::SHA256(static_cast<const unsigned char*>(memory.get()), memory.size(),
             data.m_data.data());
}

using TgBotSocket::data::DownloadFile;
using TgBotSocket::data::UploadFile;
using TgBotSocket::data::UploadFileDry;

bool SocketFile2DataHelper::DataToFile_UPLOAD_FILE_DRY(
    const void* ptr, TgBotSocket::Packet::Header::length_type len) {
    const auto* data = static_cast<const UploadFileDry*>(ptr);
    const char* filename = data->destfilepath.data();
    bool exists = false;

    LOG(INFO) << "Dry run: " << filename;
    LOG(INFO) << "Do I care if the file exists already? " << std::boolalpha
              << !data->options.overwrite;
    exists = vfs->exists(filename);
    if (!data->options.overwrite && exists) {
        LOG(WARNING) << "File already exists: " << filename;
        return false;
    }

    if (exists) {
        HashContainer hash{};
        const auto result = vfs->readFile(filename);
        if (!result) {
            LOG(ERROR) << "Failed to read from file: " << filename;
            return false;
        }

        vfs->SHA256(result.value(), hash);
        if (arraycmp(hash.m_data, data->sha256_hash)) {
            LOG(WARNING) << "File hash matches, Should I ignore? "
                         << std::boolalpha << data->options.hash_ignore;
            if (!data->options.hash_ignore) {
                return false;
            }
        } else {
            LOG(INFO) << "File hash does not match, good";
            DLOG(INFO) << hash << " is not same as incoming "
                       << HashContainer{data->sha256_hash};
        }
    }
    return true;
}
bool SocketFile2DataHelper::DataToFile_UPLOAD_FILE(
    const void* ptr, TgBotSocket::Packet::Header::length_type len) {
    const auto* data = static_cast<const UploadFile*>(ptr);

    return vfs->writeFile(data->destfilepath.data(), &data->buf[0],
                          len - sizeof(UploadFile));
}
bool SocketFile2DataHelper::DataToFile_DOWNLOAD_FILE(
    const void* ptr, TgBotSocket::Packet::Header::length_type len) {
    const auto* data = static_cast<const DownloadFile*>(ptr);

    return vfs->writeFile(data->destfilename.data(), &data->buf[0],
                          len - sizeof(DownloadFile));
}

std::optional<TgBotSocket::Packet>
SocketFile2DataHelper::DataFromFile_UPLOAD_FILE(
    const DataFromFileParam& params) {
    const auto _result = vfs->readFile(params.filepath);
    HashContainer hash{};

    if (!_result) {
        LOG(ERROR) << "Failed to read from file: " << params.filepath;
        return std::nullopt;
    }
    const auto& result = _result.value();

    // Create result packet buffer
    auto resultPointer = SharedMalloc(result.size() + sizeof(UploadFile));
    // The front bytes of the buffer is UploadFile, hence cast it
    auto* uploadFile = static_cast<UploadFile*>(resultPointer.get());
    // Copy destination file name info to the buffer
    copyTo(uploadFile->destfilepath, params.destfilepath.string().c_str());
    // Copy source file data to the buffer
    memcpy(&uploadFile->buf[0], result.get(), result.size());
    // Calculate SHA256 hash
    vfs->SHA256(result, hash);
    // Copy hash to the buffer
    uploadFile->sha256_hash = hash.m_data;
    // Copy options to the buffer
    uploadFile->options = params.options;
    // Set dry run to false
    uploadFile->options.dry_run = false;

    return TgBotSocket::Packet{TgBotSocket::Command::CMD_UPLOAD_FILE,
                               resultPointer.get(),
                               result.size() + sizeof(UploadFile)};
}

std::optional<TgBotSocket::Packet>
SocketFile2DataHelper::DataFromFile_UPLOAD_FILE_DRY(
    const DataFromFileParam& params) {
    const auto _result = vfs->readFile(params.filepath);
    HashContainer hash{};

    if (!_result) {
        LOG(ERROR) << "Failed to read from file: " << params.filepath;
        return std::nullopt;
    }
    const auto& result = _result.value();
    // Create result packet buffer
    auto resultPointer = SharedMalloc(sizeof(UploadFileDry));
    // The front bytes of the buffer is UploadFile, hence cast it
    auto* uploadFile = static_cast<UploadFileDry*>(resultPointer.get());
    // Copy destination file name info to the buffer
    copyTo(uploadFile->destfilepath, params.destfilepath.string().c_str());
    // Copy source file name to the buffer
    copyTo(uploadFile->srcfilepath, params.filepath.string().c_str());

    // Calculate SHA256 hash
    vfs->SHA256(result, hash);
    // Copy hash to the buffer
    uploadFile->sha256_hash = hash.m_data;
    // Copy options to the buffer
    uploadFile->options = params.options;
    // Set dry run to true
    uploadFile->options.dry_run = true;

    return TgBotSocket::Packet{TgBotSocket::Command::CMD_UPLOAD_FILE_DRY,
                               resultPointer.get(), sizeof(UploadFileDry)};
}

std::optional<TgBotSocket::Packet>
SocketFile2DataHelper::DataFromFile_DOWNLOAD_FILE(
    const DataFromFileParam& params) {
    const auto _result = vfs->readFile(params.filepath);
    HashContainer hash{};

    if (!_result) {
        LOG(ERROR) << "Failed to read from file: " << params.filepath;
        return std::nullopt;
    }
    const auto& result = _result.value();
    // Create result packet buffer
    auto resultPointer = SharedMalloc(result.size() + sizeof(DownloadFile));
    // The front bytes of the buffer is DownloadFile, hence cast it
    auto* downloadFile = static_cast<DownloadFile*>(resultPointer.get());
    // Copy destination file name info to the buffer
    copyTo(downloadFile->destfilename, params.destfilepath.string().c_str());
    // Copy source file data to the buffer
    memcpy(&downloadFile->buf[0], result.get(), result.size());
    // Calculate SHA256 hash
    vfs->SHA256(result, hash);

    return TgBotSocket::Packet{TgBotSocket::Command::CMD_DOWNLOAD_FILE_CALLBACK,
                               resultPointer.get(),
                               result.size() + sizeof(DownloadFile)};
}