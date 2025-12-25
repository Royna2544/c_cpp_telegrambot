#pragma once

#include <socket/api/DataStructures.hpp>
#include <cstdint>
#include <filesystem>
#include <optional>

#include <shared/FileHelperNew.hpp>

namespace TgBotSocket {

struct WriteMsgToChatId {
    ChatId chat;
    std::string message;

    static std::optional<WriteMsgToChatId> fromBuffer(
        const std::uint8_t* buffer, Packet::Header::length_type size,
        PayloadType type);
};

struct ObserveChatId {
    ChatId chat;
    bool observe;

    static std::optional<ObserveChatId> fromBuffer(
        const std::uint8_t* buffer, Packet::Header::length_type size,
        PayloadType type);
};

struct SendFileToChatId {
    ChatId chat;
    TgBotSocket::data::FileType fileType;
    std::filesystem::path filePath;

    static std::optional<SendFileToChatId> fromBuffer(
        const std::uint8_t* buffer, Packet::Header::length_type size,
        PayloadType type);
};

struct ObserveAllChats {
    bool observe;

    static std::optional<ObserveAllChats> fromBuffer(
        const std::uint8_t* buffer, Packet::Header::length_type size,
        PayloadType type);
};

struct TransferFileMeta : SocketFile2DataHelper::Params {
    static std::optional<TransferFileMeta> fromBuffer(
        const uint8_t* buffer, Packet::Header::length_type size,
        PayloadType type);
};

struct FileTransferBegin {
    std::filesystem::path destfilepath;
    uint64_t total_size;
    uint32_t chunk_size;
    std::array<uint8_t, 32> sha256_hash;

    static std::optional<FileTransferBegin> fromBuffer(
        const uint8_t* buffer, Packet::Header::length_type size,
        PayloadType type);
};

struct FileTransferChunk {
    uint32_t chunk_index;
    uint32_t chunk_data_size;
    const uint8_t* chunk_data;  // Pointer to chunk data in buffer

    static std::optional<FileTransferChunk> fromBuffer(
        const uint8_t* buffer, Packet::Header::length_type size,
        PayloadType type);
};

struct FileTransferEnd {
    bool verify_hash;

    static std::optional<FileTransferEnd> fromBuffer(
        const uint8_t* buffer, Packet::Header::length_type size,
        PayloadType type);
};

/**
 * @brief Finds the JSON byte border offset in a buffer.
 *
 * @param buffer The buffer to search.
 * @param size The size of the buffer.
 * @return The offset if found, std::nullopt otherwise.
 */
std::optional<Packet::Header::length_type> findBorderOffset(
    const uint8_t* buffer, Packet::Header::length_type size);

}  // namespace TgBotSocket