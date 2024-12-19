#include "FileHelperNew.hpp"

#include <absl/log/log.h>

#include <StructF.hpp>
#include <cstdint>

#include "PacketParser.hpp"
#include "SharedMalloc.hpp"
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

    DLOG(INFO) << "Reading from file " << filename << "...";
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
    DLOG(INFO) << "Read " << file_size << " bytes from file " << filename;
    return std::move(dataP);
}

bool RealFS::exists(const std::filesystem::path& path) {
    std::error_code errc;
    return std::filesystem::exists(path, errc);
}

void RealFS::SHA256(const SharedMalloc& memory, HashContainer& data) {
    ::SHA256(static_cast<const uint8_t*>(memory.get()), memory.size(),
             data.m_data.data());
}

using TgBotSocket::data::FileTransferMeta;

bool SocketFile2DataHelper::ReceiveTransferMeta(const Params& params) {
    const auto& filename = params.destfilepath;
    bool exists = false;

    if (!params.options.dry_run) {
        if (params.file_size == 0) {
            LOG(WARNING) << "File size is 0";
            return false;
        }
        return vfs->writeFile(params.destfilepath, params.filebuffer,
                              params.file_size);
    }

    LOG(INFO) << "Dry run: " << filename;
    LOG(INFO) << "Do I care if the file exists already? " << std::boolalpha
              << !params.options.overwrite;
    exists = vfs->exists(filename);
    if (!params.options.overwrite && exists) {
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
        if (arraycmp(hash.m_data, params.hash)) {
            LOG(WARNING) << "File hash matches, Should I ignore? "
                         << std::boolalpha << params.options.hash_ignore;
            if (!params.options.hash_ignore) {
                return false;
            }
        } else {
            LOG(INFO) << "File hash does not match, good";
            DLOG(INFO) << hash << " is not same as incoming "
                       << HashContainer{params.hash};
        }
    }
    return true;
}

std::optional<TgBotSocket::Packet> SocketFile2DataHelper::CreateTransferMeta(
    const Params& params,
    const TgBotSocket::Packet::Header::session_token_type& session_token,
    bool isRequest) {
    FileTransferMeta meta;
    SharedMalloc fileData{};

    // Copy options to the buffer
    meta.options = params.options;
    // Copy destination file name info to the buffer
    copyTo(meta.destfilepath, params.destfilepath.string());
    // Copy source file name info to the buffer
    copyTo(meta.srcfilepath, params.filepath.string());

    // If it is a request, we don't need to read the file (Just need to contain
    // src/dest meta) If it is dry run, we should read and calculate hash.
    // Otherwise, no need to read the file.
    if (isRequest) {
        return TgBotSocket::createPacket(
            TgBotSocket::Command::CMD_TRANSFER_FILE_REQUEST, &meta,
            sizeof(FileTransferMeta), TgBotSocket::PayloadType::Binary,
            session_token);
    } else {
        HashContainer hash{};

        // Read file data
        const auto _result = vfs->readFile(params.filepath);
        if (!_result) {
            LOG(ERROR) << "Failed to read from file: " << params.filepath;
            return std::nullopt;
        }
        fileData = _result.value();

        if (params.options.dry_run) {
            // Calculate SHA256 hash
            vfs->SHA256(fileData, hash);
            // Copy hash to the buffer
            meta.sha256_hash = hash.m_data;
            return TgBotSocket::createPacket(
                TgBotSocket::Command::CMD_TRANSFER_FILE, &meta,
                sizeof(FileTransferMeta), TgBotSocket::PayloadType::Binary,
                session_token);
        }
        meta.options.hash_ignore = true;

        // Create result packet buffer
        auto resultPointer =
            SharedMalloc(fileData.size() + sizeof(FileTransferMeta));

        // Copy meta data to the buffer
        resultPointer.assignFrom(meta);
        // Copy source file data to the buffer
        resultPointer.assignFrom(fileData.get(), fileData.size(),
                                 sizeof(FileTransferMeta));

        return TgBotSocket::createPacket(
            TgBotSocket::Command::CMD_TRANSFER_FILE, resultPointer.get(),
            resultPointer.size(), TgBotSocket::PayloadType::Binary,
            session_token);
    }
}
