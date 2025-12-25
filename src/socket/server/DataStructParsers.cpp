#include "DataStructParsers.hpp"

#include <absl/log/log.h>
#include <absl/strings/escaping.h>
#include <openssl/sha.h>

#include <shared/PacketParser.hpp>

namespace TgBotSocket {

std::optional<WriteMsgToChatId> WriteMsgToChatId::fromBuffer(
    const std::uint8_t* buffer, TgBotSocket::Packet::Header::length_type size,
    TgBotSocket::PayloadType type) {
    WriteMsgToChatId result{};
    switch (type) {
        case PayloadType::Binary:
            if (size != sizeof(data::WriteMsgToChatId)) {
                DLOG(WARNING) << "Payload size mismatch on WriteMsgToChatId";
                return std::nullopt;
            }
            {
                const auto* data =
                    reinterpret_cast<const data::WriteMsgToChatId*>(buffer);
                result.chat = data->chat;
                result.message = safeParse(data->message);
            }
            return result;
        case PayloadType::Json: {
            auto _root = parseAndCheck(buffer, size, {"chat", "message"});
            if (!_root) {
                return std::nullopt;
            }
            auto& root = _root.value();
            result.chat = root["chat"].get<ChatId>();
            result.message = root["message"].get<std::string>();
            return result;
        }
        default:
            LOG(ERROR) << "Invalid payload type for WriteMsgToChatId";
            return std::nullopt;
    }
}

std::optional<ObserveChatId> ObserveChatId::fromBuffer(
    const std::uint8_t* buffer, TgBotSocket::Packet::Header::length_type size,
    TgBotSocket::PayloadType type) {
    ObserveChatId result{};
    switch (type) {
        case PayloadType::Binary:
            if (size != sizeof(data::ObserveChatId)) {
                DLOG(WARNING) << "Payload size mismatch on ObserveChatId";
                return std::nullopt;
            }
            {
                const auto* data =
                    reinterpret_cast<const data::ObserveChatId*>(buffer);
                result.chat = data->chat;
                result.observe = data->observe;
            }
            return result;
        case PayloadType::Json: {
            auto _root = parseAndCheck(buffer, size, {"chat", "observe"});
            if (!_root) {
                return std::nullopt;
            }
            auto& root = _root.value();
            result.chat = root["chat"].get<ChatId>();
            result.observe = root["observe"].get<bool>();
            return result;
        }
        default:
            LOG(ERROR) << "Invalid payload type for ObserveChatId";
            return std::nullopt;
    }
}

std::optional<SendFileToChatId> SendFileToChatId::fromBuffer(
    const std::uint8_t* buffer, TgBotSocket::Packet::Header::length_type size,
    TgBotSocket::PayloadType type) {
    SendFileToChatId result{};
    switch (type) {
        case PayloadType::Binary:
            if (size < sizeof(data::SendFileToChatId)) {
                DLOG(WARNING) << "Payload size mismatch on SendFileToChatId";
                return std::nullopt;
            }
            {
                const auto* data =
                    reinterpret_cast<const data::SendFileToChatId*>(buffer);
                result.chat = data->chat;
                result.fileType = data->fileType;
                result.filePath = safeParse(data->filePath);
            }
            return result;
        case PayloadType::Json: {
            auto _root =
                parseAndCheck(buffer, size, {"chat", "fileType", "filePath"});
            if (!_root) {
                return std::nullopt;
            }
            auto& root = _root.value();
            result.chat = root["chat"].get<ChatId>();
            result.fileType = static_cast<TgBotSocket::data::FileType>(
                root["fileType"].get<int>());
            result.filePath = root["filePath"].get<std::string>();
            return result;
        }
        default:
            LOG(ERROR) << "Invalid payload type for SendFileToChatId";
            return std::nullopt;
    }
}

std::optional<ObserveAllChats> ObserveAllChats::fromBuffer(
    const std::uint8_t* buffer, TgBotSocket::Packet::Header::length_type size,
    TgBotSocket::PayloadType type) {
    ObserveAllChats result{};
    switch (type) {
        case PayloadType::Binary:
            if (size != sizeof(data::ObserveAllChats)) {
                DLOG(WARNING)
                    << "Payload size mismatch on ObserveAllChats for size: "
                    << size;
                return std::nullopt;
            }
            {
                const auto* data =
                    reinterpret_cast<const data::ObserveAllChats*>(buffer);
                result.observe = data->observe;
            }
            return result;
        case PayloadType::Json: {
            auto _root = parseAndCheck(buffer, size, {"observe"});
            if (!_root) {
                return std::nullopt;
            }
            auto& root = _root.value();
            result.observe = root["observe"].get<bool>();
            return result;
        }
        default:
            LOG(ERROR) << "Invalid payload type for ObserveAllChats";
            return std::nullopt;
    }
}

std::optional<Packet::Header::length_type> findBorderOffset(
    const uint8_t* buffer, Packet::Header::length_type size) {
    Packet::Header::length_type offset = 0;
    for (Packet::Header::length_type i = 0; i < size; ++i) {
        if (buffer[i] == data::JSON_BYTE_BORDER) {
            LOG(INFO) << "Found JSON_BYTE_BORDER in offset " << i;
            return i;
        }
    }
    LOG(WARNING) << "JSON_BYTE_BORDER not found in buffer";
    return std::nullopt;
}

std::optional<TransferFileMeta> TransferFileMeta::fromBuffer(
    const uint8_t* buffer, TgBotSocket::Packet::Header::length_type size,
    TgBotSocket::PayloadType type) {
    TransferFileMeta result{};
    switch (type) {
        case PayloadType::Binary: {
            if (size < sizeof(data::FileTransferMeta)) {
                DLOG(WARNING) << "Payload size mismatch on UploadFileMeta";
                return std::nullopt;
            }
            {
                const auto* data =
                    reinterpret_cast<const data::FileTransferMeta*>(buffer);
                result.filepath = safeParse(data->srcfilepath);
                result.destfilepath = safeParse(data->destfilepath);
                result.options = data->options;
                result.hash = data->sha256_hash;
                result.file_size = size - sizeof(data::FileTransferMeta);
                result.filebuffer = buffer + sizeof(data::FileTransferMeta);
            }
            return result;
        }
        case PayloadType::Json: {
            const auto offset = findBorderOffset(buffer, size).value_or(size);
            std::string json(reinterpret_cast<const char*>(buffer), offset);
            auto _root =
                parseAndCheck(buffer, size, {"srcfilepath", "destfilepath"});
            if (!_root) {
                return std::nullopt;
            }
            auto& root = _root.value();
            result.filepath = root["srcfilepath"].get<std::string>();
            result.destfilepath = root["destfilepath"].get<std::string>();
            data::FileTransferMeta::Options options;
            options.overwrite = root["options"]["overwrite"].get<bool>();
            options.hash_ignore = root["options"]["hash_ignore"].get<bool>();
            options.dry_run = root["options"]["dry_run"].get<bool>();
            if (!options.hash_ignore && !root.contains("hash")) {
                LOG(WARNING)
                    << "hash_ignore is false, but hash is not provided.";
                return std::nullopt;
            }
            if (root.contains("hash")) {
                auto parsed = hexDecode<SHA256_DIGEST_LENGTH>(
                    root["hash"].get<std::string>());
                if (!parsed) {
                    return std::nullopt;
                }
                result.hash = parsed.value();
            }
            result.options = options;
            result.file_size = size - offset;
            result.filebuffer = buffer + offset;
            return result;
        }
        default:
            LOG(ERROR) << "Invalid payload type for TransferFileMeta";
            return std::nullopt;
    }
}

std::optional<FileTransferBegin> FileTransferBegin::fromBuffer(
    const uint8_t* buffer, TgBotSocket::Packet::Header::length_type size,
    TgBotSocket::PayloadType type) {
    FileTransferBegin result{};
    switch (type) {
        case PayloadType::Binary:
            if (size != sizeof(data::FileTransferBegin)) {
                DLOG(WARNING) << "Payload size mismatch on FileTransferBegin";
                return std::nullopt;
            }
            {
                const auto* data =
                    reinterpret_cast<const data::FileTransferBegin*>(buffer);
                result.destfilepath = safeParse(data->destfilepath);
                result.total_size = data->total_size;
                result.chunk_size = data->chunk_size;
                std::ranges::copy(data->sha256_hash, result.sha256_hash.begin());
            }
            return result;
        case PayloadType::Json: {
            auto _root = parseAndCheck(buffer, size,
                                       {"destfilepath", "total_size", "chunk_size", "sha256_hash"});
            if (!_root) {
                return std::nullopt;
            }
            auto& root = _root.value();
            result.destfilepath = root["destfilepath"].get<std::string>();
            result.total_size = root["total_size"].get<uint64_t>();
            result.chunk_size = root["chunk_size"].get<uint32_t>();
            
            auto parsed = hexDecode<32>(root["sha256_hash"].get<std::string>());
            if (!parsed) {
                LOG(ERROR) << "Failed to decode SHA256 hash";
                return std::nullopt;
            }
            result.sha256_hash = parsed.value();
            return result;
        }
        default:
            LOG(ERROR) << "Invalid payload type for FileTransferBegin";
            return std::nullopt;
    }
}

std::optional<FileTransferChunk> FileTransferChunk::fromBuffer(
    const uint8_t* buffer, TgBotSocket::Packet::Header::length_type size,
    TgBotSocket::PayloadType type) {
    FileTransferChunk result{};
    switch (type) {
        case PayloadType::Binary:
            if (size < sizeof(data::FileTransferChunk)) {
                DLOG(WARNING) << "Payload size mismatch on FileTransferChunk";
                return std::nullopt;
            }
            {
                const auto* data =
                    reinterpret_cast<const data::FileTransferChunk*>(buffer);
                result.chunk_index = data->chunk_index;
                result.chunk_data_size = data->chunk_data_size;
                result.chunk_data = buffer + sizeof(data::FileTransferChunk);
                
                // Verify we have enough data
                if (size < sizeof(data::FileTransferChunk) + result.chunk_data_size) {
                    LOG(ERROR) << "Chunk data size mismatch";
                    return std::nullopt;
                }
            }
            return result;
        case PayloadType::Json: {
            const auto offset = findBorderOffset(buffer, size).value_or(size);
            auto _root = parseAndCheck(buffer, offset,
                                       {"chunk_index", "chunk_data_size"});
            if (!_root) {
                return std::nullopt;
            }
            auto& root = _root.value();
            result.chunk_index = root["chunk_index"].get<uint32_t>();
            result.chunk_data_size = root["chunk_data_size"].get<uint32_t>();
            result.chunk_data = buffer + offset;
            
            // Verify we have enough data
            if (size < offset + result.chunk_data_size) {
                LOG(ERROR) << "Chunk data size mismatch";
                return std::nullopt;
            }
            return result;
        }
        default:
            LOG(ERROR) << "Invalid payload type for FileTransferChunk";
            return std::nullopt;
    }
}

std::optional<FileTransferEnd> FileTransferEnd::fromBuffer(
    const uint8_t* buffer, TgBotSocket::Packet::Header::length_type size,
    TgBotSocket::PayloadType type) {
    FileTransferEnd result{};
    switch (type) {
        case PayloadType::Binary:
            if (size != sizeof(data::FileTransferEnd)) {
                DLOG(WARNING) << "Payload size mismatch on FileTransferEnd";
                return std::nullopt;
            }
            {
                const auto* data =
                    reinterpret_cast<const data::FileTransferEnd*>(buffer);
                result.verify_hash = data->verify_hash;
            }
            return result;
        case PayloadType::Json: {
            auto _root = parseAndCheck(buffer, size, {"verify_hash"});
            if (!_root) {
                return std::nullopt;
            }
            auto& root = _root.value();
            result.verify_hash = root["verify_hash"].get<bool>();
            return result;
        }
        default:
            LOG(ERROR) << "Invalid payload type for FileTransferEnd";
            return std::nullopt;
    }
}

}  // namespace TgBotSocket