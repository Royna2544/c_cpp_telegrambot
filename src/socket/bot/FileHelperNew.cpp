#include "FileHelperNew.hpp"

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/strings/escaping.h>
#include <json/value.h>
#include <json/writer.h>

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

namespace {

Json::Value fromMeta(const FileTransferMeta& meta) {
    Json::Value root;
    // Below two are guaranteed to be null-terminated
    root["srcfilepath"] = meta.srcfilepath.data();
    root["destfilepath"] = meta.destfilepath.data();
    root["options"]["dry_run"] = meta.options.dry_run;
    root["options"]["hash_ignore"] = meta.options.hash_ignore;
    root["options"]["overwrite"] = meta.options.overwrite;
    if (!meta.options.hash_ignore) {
        root["hash"] = TgBotSocket::hexEncode(meta.sha256_hash);
    }
    return root;
}

TgBotSocket::Packet fromMeta(
    const TgBotSocket::Command cmd, const FileTransferMeta& meta,
    const TgBotSocket::PayloadType& type,
    const TgBotSocket::Packet::Header::session_token_type& session_token) {
    switch (type) {
        case TgBotSocket::PayloadType::Binary: {
            return TgBotSocket::createPacket(
                cmd, &meta, sizeof(FileTransferMeta), type, session_token);
        }
        case TgBotSocket::PayloadType::Json: {
            return TgBotSocket::nodeToPacket(cmd, fromMeta(meta),
                                             session_token);
        }
        default:
            LOG(ERROR) << "Unknown payload type";
            return {};
    }
}

}  // namespace

std::optional<TgBotSocket::Packet> SocketFile2DataHelper::CreateTransferMeta(
    const Params& params,
    const TgBotSocket::Packet::Header::session_token_type& session_token,
    const TgBotSocket::PayloadType type, bool isRequest) {
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
        return fromMeta(TgBotSocket::Command::CMD_TRANSFER_FILE_REQUEST, meta,
                        type, session_token);
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
            return fromMeta(TgBotSocket::Command::CMD_TRANSFER_FILE, meta, type,
                            session_token);
        }
        meta.options.hash_ignore = true;

        // Create result packet buffer
        size_t allocSize = fileData.size() + sizeof(FileTransferMeta);
        if (type != TgBotSocket::PayloadType::Binary) {
            allocSize += 1;  // For the border
        }
        SharedMalloc resultPointer(allocSize);

        size_t fileOffset = 0;
        switch (type) {
            case TgBotSocket::PayloadType::Json: {
                Json::FastWriter writer;
                std::string jsonMeta = writer.write(fromMeta(meta));
                resultPointer.assignFrom(jsonMeta.c_str(), jsonMeta.size());
                // Copy border to the buffer
                resultPointer.assignFrom(TgBotSocket::data::JSON_BYTE_BORDER,
                                         jsonMeta.size());
                fileOffset = jsonMeta.size() + 1;
                break;
            }
            case TgBotSocket::PayloadType::Binary: {
                // Copy meta data to the buffer
                resultPointer.assignFrom(meta);
                fileOffset = sizeof(FileTransferMeta);
                break;
            }
        }
        ABSL_ASSERT(fileOffset != 0);

        // Copy source file data to the buffer
        resultPointer.assignFrom(fileData.get(), fileData.size(), fileOffset);

        return TgBotSocket::createPacket(
            TgBotSocket::Command::CMD_TRANSFER_FILE, resultPointer.get(),
            resultPointer.size(), type, session_token);
    }
}
