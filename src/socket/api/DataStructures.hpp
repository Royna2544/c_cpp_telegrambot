#pragma once

#include "ByteHelper.hpp"
#include "CoreTypes.hpp"
#include "Packet.hpp"
#include "Utilities.hpp"

#include <api/typedefs.h>

namespace TgBotSocket::data {

/**
 * @brief File type enumeration for media uploads
 */
enum class FileType {
    TYPE_PHOTO = 0,
    TYPE_VIDEO = 1,
    TYPE_GIF = 2,
    TYPE_DOCUMENT = 3,
    TYPE_DICE = 4,
    TYPE_STICKER = 5,
    TYPE_MAX
};

/**
 * @brief Spam blocking control modes
 */
enum class CtrlSpamBlock {
    OFF = 0,           ///< Disabled
    LOGGING_ONLY = 1,  ///< Log only, no action
    PURGE = 2,         ///< Delete messages only
    PURGE_AND_MUTE = 3,///< Delete and mute sender
    MAX,
};

/**
 * @brief JSON border byte for separating metadata from binary data
 */
constexpr uint8_t JSON_BYTE_BORDER = 0xFF;

/**
 * @brief Write message to chat command data
 * 
 * JSON schema: { "chat": int64, "message": string }
 */
struct alignas(Limits::ALIGNMENT) WriteMsgToChatId {
    ByteHelper<ChatId> chat;
    MessageStringArray message;
};

/**
 * @brief Observe single chat command data
 * 
 * JSON schema: { "chat": int64, "observe": bool }
 */
struct alignas(Limits::ALIGNMENT) ObserveChatId {
    ByteHelper<ChatId> chat;
    bool observe;
};

/**
 * @brief Send file to chat command data
 * 
 * JSON schema: { "chat": int64, "fileType": int, "filePath": string }
 */
struct alignas(Limits::ALIGNMENT) SendFileToChatId {
    ByteHelper<ChatId> chat;
    ByteHelper<FileType> fileType;
    alignas(Limits::ALIGNMENT) PathStringArray filePath;
};

/**
 * @brief Observe all chats command data
 * 
 * JSON schema: { "observe": bool }
 */
struct alignas(Limits::ALIGNMENT) ObserveAllChats {
    bool observe;
};

/**
 * @brief File transfer metadata with options
 * 
 * For JSON: metadata + 0xFF border + raw bytes
 * For Binary: struct + raw bytes
 */
struct alignas(Limits::ALIGNMENT) FileTransferMeta {
    PathStringArray srcfilepath{};
    PathStringArray destfilepath{};
    SHA256StringArray sha256_hash{};

    struct Options {
        bool overwrite = false;
        bool hash_ignore = false;
        bool dry_run = false;

        constexpr bool operator==(const Options& other) const {
            return overwrite == other.overwrite &&
                   hash_ignore == other.hash_ignore && 
                   dry_run == other.dry_run;
        }
    } options;

    bool operator==(const FileTransferMeta& other) const {
        return arraycmp(destfilepath, other.destfilepath) &&
               arraycmp(srcfilepath, other.srcfilepath) &&
               arraycmp(sha256_hash, other.sha256_hash) && 
               options == other.options;
    }
};

/**
 * @brief Chunked file transfer session begin
 * 
 * JSON schema: { "destfilepath": string, "total_size": uint64, "chunk_size": uint32, "sha256_hash": string }
 */
struct alignas(Limits::ALIGNMENT) FileTransferBegin {
    PathStringArray destfilepath{};
    ByteHelper<uint64_t> total_size;
    ByteHelper<uint32_t> chunk_size;
    SHA256StringArray sha256_hash{};
};

/**
 * @brief Chunked file transfer chunk data
 * 
 * JSON schema: { "chunk_index": uint32, "chunk_data_size": uint32 } + raw bytes
 * Binary: struct + raw chunk bytes
 */
struct alignas(Limits::ALIGNMENT) FileTransferChunk {
    ByteHelper<uint32_t> chunk_index;
    ByteHelper<uint32_t> chunk_data_size;
    // Followed by chunk_data_size bytes of actual data
};

/**
 * @brief Chunked file transfer chunk response
 * 
 * JSON schema: { "chunk_index": uint32, "success": bool, "error_msg": string }
 */
struct alignas(Limits::ALIGNMENT) FileTransferChunkResponse {
    ByteHelper<uint32_t> chunk_index;
    bool success;
    alignas(Limits::ALIGNMENT) MessageStringArray error_msg{};
};

/**
 * @brief Chunked file transfer session end
 * 
 * JSON schema: { "verify_hash": bool }
 */
struct alignas(Limits::ALIGNMENT) FileTransferEnd {
    bool verify_hash;
};

}  // namespace TgBotSocket::data