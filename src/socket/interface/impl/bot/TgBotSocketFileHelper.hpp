#pragma once

#include <absl/log/absl_log.h>
#include <openssl/sha.h>

#include <TgBotSocket_Export.hpp>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <ios>
#include <memory>
#include <optional>
#include <string>
#include <system_error>

#include "../../../../include/StructF.hpp"

namespace FileDataHelper {
using len_t = TgBotSocket::PacketHeader::length_type;
using RAIIMalloc = std::unique_ptr<void, decltype(&free)>;

enum Pass {
    UPLOAD_FILE_DRY,
    UPLOAD_FILE,
    DOWNLOAD_FILE,
};

struct DataFromFileParam {
    std::filesystem::path filepath;
    std::filesystem::path destfilepath;
    TgBotSocket::data::UploadFile::Options options;
};
struct ReadFileFullyRet {
    RAIIMalloc data;
    len_t size;
};

static RAIIMalloc createMemory(len_t size);
static bool writeFileCommon(const std::filesystem::path& filename,
                            const uint8_t* startAddr, const len_t size);

static std::optional<ReadFileFullyRet> readFileFullyCommon(
    const std::filesystem::path& filename);

template <Pass p>
static bool DataToFile(const void* ptr, len_t len) = delete;
// -> Server (dry)
template <>
bool DataToFile<UPLOAD_FILE_DRY>(const void* ptr, len_t len);
// -> Server
template <>
bool DataToFile<UPLOAD_FILE>(const void* ptr, len_t len);
// -> Client
template <>
bool DataToFile<DOWNLOAD_FILE>(const void* ptr, len_t len);

template <Pass p>
static std::optional<TgBotSocket::Packet> DataFromFile(
    const DataFromFileParam& params) = delete;

// Client ->
template <>
std::optional<TgBotSocket::Packet> DataFromFile<UPLOAD_FILE>(
    const DataFromFileParam& params);

// Client ->
template <>
std::optional<TgBotSocket::Packet> DataFromFile<UPLOAD_FILE_DRY>(
    const DataFromFileParam& params);

// Server ->
template <>
std::optional<TgBotSocket::Packet> DataFromFile<DOWNLOAD_FILE>(
    const DataFromFileParam& params);
};  // namespace FileDataHelper

// Implementation starts
using TgBotSocket::data::DownloadFile;
using TgBotSocket::data::UploadFile;
using TgBotSocket::data::UploadFileDry;
struct HashContainer {
    std::array<unsigned char, SHA256_DIGEST_LENGTH> m_data;
};

FileDataHelper::RAIIMalloc FileDataHelper::createMemory(len_t size) {
    return {malloc(size), &free};
}

bool FileDataHelper::writeFileCommon(const std::filesystem::path& filename,
                                     const uint8_t* startAddr,
                                     const len_t size) {
    size_t writtenCnt = 0;
    F file;

    ABSL_LOG(INFO) << "Writing to file " << filename << "...";
    if (!file.open(filename.string(), F::Mode::WriteBinary)) {
        ABSL_LOG(ERROR) << "Failed to open file";
        return false;
    }
    if (!file.write(startAddr, size, 1)) {
        ABSL_LOG(ERROR) << "Failed to write to file";
        return false;
    }
    ABSL_LOG(INFO) << "Wrote " << size << " bytes to file " << filename;
    return true;
}

std::optional<FileDataHelper::ReadFileFullyRet>
FileDataHelper::readFileFullyCommon(const std::filesystem::path& filename) {
    std::error_code errc;
    F file;
    const auto file_size = std::filesystem::file_size(filename, errc);

    if (errc) {
        ABSL_LOG(ERROR) << "Failed to get file size: " << filename << ": "
                        << errc.message();
        return std::nullopt;
    }
    if (!file.open(filename.string(), F::Mode::ReadBinary)) {
        ABSL_LOG(ERROR) << "Failed to fopen file: " << filename;
        return std::nullopt;
    }
    auto dataP = createMemory(file_size);
    if (!file.read(dataP.get(), file_size, 1)) {
        ABSL_LOG(ERROR) << "Failed to read from file";
        return std::nullopt;
    }
    return ReadFileFullyRet{std::move(dataP), file_size};
}

inline std::ostream& operator<<(std::ostream& self, const HashContainer& data) {
    for (const auto& c : data.m_data) {
        self << std::hex << std::setw(2) << std::setfill('0')
             << static_cast<int>(c);
    }
    return self;
}

template <>
bool FileDataHelper::DataToFile<FileDataHelper::UPLOAD_FILE_DRY>(
    const void* ptr, len_t len) {
    const auto* data = static_cast<const UploadFileDry*>(ptr);
    const char* filename = data->destfilepath.data();
    std::error_code errc;
    bool exists = false;

    ABSL_LOG(INFO) << "Dry run: " << filename;
    ABSL_LOG(INFO) << "Do I care if the file exists already? " << std::boolalpha
                   << !data->options.overwrite;
    exists = std::filesystem::exists(filename, errc);
    if (!data->options.overwrite && exists) {
        ABSL_LOG(WARNING) << "File already exists: " << filename;
        return false;
    }

    if (exists) {
        HashContainer hash{};

        if (errc) {
            ABSL_LOG(ERROR) << "Failed to get file size: " << filename << ": "
                            << errc.message();
            return false;
        }
        const auto result = readFileFullyCommon(filename);
        if (!result) {
            ABSL_LOG(ERROR) << "Failed to read from file: " << filename;
            return false;
        }

        SHA256(static_cast<const unsigned char*>(result->data.get()),
               result->size, hash.m_data.data());
        if (memcmp(hash.m_data.data(), data->sha256_hash.data(),
                   SHA256_DIGEST_LENGTH) == 0) {
            ABSL_LOG(WARNING) << "File hash matches, Should I ignore? "
                              << std::boolalpha << data->options.hash_ignore;
            if (!data->options.hash_ignore) {
                return false;
            }
        } else {
            ABSL_LOG(INFO) << "File hash does not match, good";
            ABSL_DLOG(INFO) << hash << " is not same as incoming "
                            << HashContainer{data->sha256_hash};
        }
    }
    return true;
}

template <>
bool FileDataHelper::DataToFile<FileDataHelper::UPLOAD_FILE>(const void* ptr,
                                                             len_t len) {
    const auto* data = static_cast<const UploadFile*>(ptr);

    return writeFileCommon(data->destfilepath.data(), &data->buf[0],
                           len - sizeof(UploadFile));
}

template <>
bool FileDataHelper::DataToFile<FileDataHelper::DOWNLOAD_FILE>(const void* ptr,
                                                               len_t len) {
    const auto* data = static_cast<const DownloadFile*>(ptr);

    return writeFileCommon(data->destfilename.data(), &data->buf[0],
                           len - sizeof(DownloadFile));
}

template <>
std::optional<TgBotSocket::Packet>
FileDataHelper::DataFromFile<FileDataHelper::UPLOAD_FILE>(
    const DataFromFileParam& params) {
    const auto result = readFileFullyCommon(params.filepath);
    HashContainer hash{};

    if (!result) {
        ABSL_LOG(ERROR) << "Failed to read from file: " << params.filepath;
        return std::nullopt;
    }
    // Create result packet buffer
    auto resultPointer = createMemory(result->size + sizeof(UploadFile));
    // The front bytes of the buffer is UploadFile, hence cast it
    auto* uploadFile = static_cast<UploadFile*>(resultPointer.get());
    // Copy destination file name info to the buffer
    copyTo(uploadFile->destfilepath, params.destfilepath.string().c_str());
    // Copy source file data to the buffer
    memcpy(&uploadFile->buf[0], result->data.get(), result->size);
    // Calculate SHA256 hash
    SHA256(static_cast<unsigned char*>(result->data.get()), result->size,
           hash.m_data.data());
    // Copy hash to the buffer
    uploadFile->sha256_hash = hash.m_data;
    // Copy options to the buffer
    uploadFile->options = params.options;
    // Set dry run to false
    uploadFile->options.dry_run = false;

    return TgBotSocket::Packet{TgBotSocket::Command::CMD_UPLOAD_FILE,
                               resultPointer.get(),
                               result->size + sizeof(UploadFile)};
}

template <>
std::optional<TgBotSocket::Packet>
FileDataHelper::DataFromFile<FileDataHelper::UPLOAD_FILE_DRY>(
    const DataFromFileParam& params) {
    const auto result = readFileFullyCommon(params.filepath);
    HashContainer hash{};

    if (!result) {
        ABSL_LOG(ERROR) << "Failed to read from file: " << params.filepath;
        return std::nullopt;
    }
    // Create result packet buffer
    auto resultPointer = createMemory(sizeof(UploadFileDry));
    // The front bytes of the buffer is UploadFile, hence cast it
    auto* uploadFile = static_cast<UploadFileDry*>(resultPointer.get());
    // Copy destination file name info to the buffer
    copyTo(uploadFile->destfilepath, params.destfilepath.string().c_str());
    // Copy source file name to the buffer
    copyTo(uploadFile->srcfilepath, params.filepath.string().c_str());

    // Calculate SHA256 hash
    SHA256(static_cast<unsigned char*>(result->data.get()), result->size,
           hash.m_data.data());
    // Copy hash to the buffer
    uploadFile->sha256_hash = hash.m_data;
    // Copy options to the buffer
    uploadFile->options = params.options;
    // Set dry run to true
    uploadFile->options.dry_run = true;

    return TgBotSocket::Packet{TgBotSocket::Command::CMD_UPLOAD_FILE_DRY,
                               resultPointer.get(), sizeof(UploadFileDry)};
}

// Server ->
template <>
std::optional<TgBotSocket::Packet>
FileDataHelper::DataFromFile<FileDataHelper::DOWNLOAD_FILE>(
    const DataFromFileParam& params) {
    const auto result = readFileFullyCommon(params.filepath);
    std::array<unsigned char, SHA256_DIGEST_LENGTH> hash{};

    if (!result) {
        ABSL_LOG(ERROR) << "Failed to read from file: " << params.filepath;
        return std::nullopt;
    }
    // Create result packet buffer
    auto resultPointer = createMemory(result->size + sizeof(DownloadFile));
    // The front bytes of the buffer is DownloadFile, hence cast it
    auto* downloadFile = static_cast<DownloadFile*>(resultPointer.get());
    // Copy destination file name info to the buffer
    copyTo(downloadFile->destfilename, params.destfilepath.string().c_str());
    // Copy source file data to the buffer
    memcpy(&downloadFile->buf[0], result->data.get(), result->size);
    // Calculate SHA256 hash
    SHA256(static_cast<unsigned char*>(result->data.get()), result->size,
           hash.data());

    return TgBotSocket::Packet{TgBotSocket::Command::CMD_DOWNLOAD_FILE_CALLBACK,
                               resultPointer.get(),
                               result->size + sizeof(DownloadFile)};
}
