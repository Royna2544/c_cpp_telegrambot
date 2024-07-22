#pragma once

// A header export for the TgBot's socket connection

#include <openssl/sha.h>
#include <zlib.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

#include "../../include/SharedMalloc.hpp"
#include "../../include/Types.h"

namespace TgBotSocket {

constexpr int MAX_PATH_SIZE = 256;
constexpr int MAX_MSG_SIZE = 256;
constexpr int ALIGNMENT = 8;

enum class Command : std::int32_t {
    CMD_WRITE_MSG_TO_CHAT_ID,
    CMD_CTRL_SPAMBLOCK,
    CMD_OBSERVE_CHAT_ID,
    CMD_SEND_FILE_TO_CHAT_ID,
    CMD_OBSERVE_ALL_CHATS,
    CMD_DELETE_CONTROLLER_BY_ID,
    CMD_GET_UPTIME,
    CMD_UPLOAD_FILE,
    CMD_DOWNLOAD_FILE,
    CMD_CLIENT_MAX,

    // Below are internal commands
    CMD_SERVER_INTERNAL_START = 100,
    CMD_GET_UPTIME_CALLBACK = CMD_SERVER_INTERNAL_START,
    CMD_GENERIC_ACK,
    CMD_UPLOAD_FILE_DRY,
    CMD_UPLOAD_FILE_DRY_CALLBACK,
    CMD_DOWNLOAD_FILE_CALLBACK,
    CMD_MAX,
};

/**
 * @brief Header for TgBotCommand Packets
 *
 * Header contains the magic value, command, and the size of the data
 */
 
struct alignas(ALIGNMENT) PacketHeader {
    using length_type = uint64_t;
    constexpr static int64_t MAGIC_VALUE_BASE = 0xDEADFACE;
    // Version number, to be increased on breaking changes
    // 1: Initial version
    // 2: Added crc32 checks to packet data
    // 3: Uploadfile has a sha256sum check, std::array conversions
    // 4: Move CMD_UPLOAD_FILE_DRY to internal namespace
    // 5: Use the packed attribute for structs
    // 6: Make CMD_UPLOAD_FILE_DRY_CALLBACK return sperate callback, and add srcpath to UploadFile
    // 7: Remove packed attribute, align all as 8 bytes
    constexpr static int DATA_VERSION = 7;
    constexpr static int64_t MAGIC_VALUE = MAGIC_VALUE_BASE + DATA_VERSION;

    int64_t magic = MAGIC_VALUE;  ///< Magic value to verify the packet
    Command cmd{};                ///< Command to be executed
    [[maybe_unused]] uint32_t padding1;            ///< Padding to align `data_size` to 8 bytes
    length_type data_size{};      ///< Size of the data in the packet
    uint32_t checksum{};          ///< Checksum of the packet data
    [[maybe_unused]] uint32_t padding2;            ///< Padding to ensure struct size is a multiple of 8 bytes
};

/**
 * @brief Packet for sending commands to the server
 *
 * @code data_ptr is only vaild for this scope: This should not be sent, instead
 * it must be memcpy'd
 *
 * This packet is used to send commands to the server.
 * It contains a header, which contains the magic value, the command, and the
 * size of the data. The data is then followed by the actual data.
 */
struct alignas(ALIGNMENT) Packet {
    static constexpr auto hdr_sz = sizeof(PacketHeader);
    using header_type = PacketHeader;
    header_type header{};
    SharedMalloc data;

    explicit Packet(header_type::length_type length) : data(length) {
        header.magic = header_type::MAGIC_VALUE;
        header.data_size = length;
    }

    // Constructor that takes malloc
    template <typename T>
    explicit Packet(Command cmd, T data) : Packet(cmd, &data, sizeof(T)) {
        static_assert(!std::is_pointer_v<T>,
                      "This constructor should not be used with a pointer");
    }

    // Constructor that takes pointer, uses malloc but with size
    template <typename T>
    explicit Packet(Command cmd, T in_data, header_type::length_type size) : data(size) {
        static_assert(std::is_pointer_v<T>,
                      "This constructor should not be used with non pointer");
        header.cmd = cmd;
        header.magic = header_type::MAGIC_VALUE;
        header.data_size = size;
        data.assignFrom(in_data, header.data_size);
        uLong crc = crc32(0L, Z_NULL, 0);  // Initial value
        header.checksum =
            crc32(crc, reinterpret_cast<Bytef*>(data.get()), header.data_size);
    }

    // Converts to full SocketData object, including header
    SharedMalloc toSocketData() {
        data->realloc(hdr_sz + header.data_size);
        memmove(static_cast<char*>(data.get()) + hdr_sz, data.get(),
                header.data_size);
        memcpy(data.get(), &header, hdr_sz);
        return data;
    }
};

using PathStringArray = std::array<char, MAX_PATH_SIZE>;
using MessageStringArray = std::array<char, MAX_MSG_SIZE>;
using SHA256StringArray = std::array<unsigned char, SHA256_DIGEST_LENGTH>;

namespace data {

enum class FileType {
    TYPE_PHOTO,
    TYPE_VIDEO,
    TYPE_GIF,
    TYPE_DOCUMENT,
    TYPE_DICE,
    TYPE_STICKER,
    TYPE_MAX
};

enum class CtrlSpamBlock {
    CTRL_OFF,              // Disabled
    CTRL_LOGGING_ONLY_ON,  // Logging only, not taking action
    CTRL_ON,               // Enabled, does delete but doesn't mute
    CTRL_ENFORCE,          // Enabled, deletes and mutes
    CTRL_MAX,
};

struct alignas(ALIGNMENT)  WriteMsgToChatId {
    ChatId chat;                             // destination chatid
    MessageStringArray message;  // Msg to send
};

struct alignas(ALIGNMENT) ObserveChatId {
    ChatId chat;
    bool observe;  // new state for given ChatId,
                   // true/false - Start/Stop observing
};

struct alignas(ALIGNMENT) SendFileToChatId {
    ChatId chat;                               // Destination ChatId
    FileType fileType;                         // File type for file
    PathStringArray filePath;  // Path to file (local)
};

struct alignas(ALIGNMENT) ObserveAllChats {
    bool observe;  // new state for all chats,
                   // true/false - Start/Stop observing
};

struct alignas(ALIGNMENT) DeleteControllerById {
    int controller_id;
};

struct alignas(ALIGNMENT) UploadFileDry {
    PathStringArray destfilepath{};   // Destination file name 
    PathStringArray srcfilepath{};    // Source file name (This is not used on the remote, used if dry=true)
    SHA256StringArray sha256_hash{};  // SHA256 hash of the file

    // Returns AckType::ERROR_COMMAND_IGNORED on options failure
    struct Options {
        // Overwrite existing file if exists
        bool overwrite = false;  

        // Ignore hash, always upload file even if the same file
        // exists and the hash matches. Depends on overwrite flag
        // for actual overwriting
        bool hash_ignore = false;

        // If true, just check the hashes and return result.  
        bool dry_run = false;
    } options;
};

struct alignas(ALIGNMENT) UploadFile : public UploadFileDry {
    using Options = UploadFileDry::Options;
    uint8_t buf[];  // Buffer
};

struct alignas(ALIGNMENT) DownloadFile {
    PathStringArray filepath{};      // Path to file (in remote)
    PathStringArray destfilename{};  // Destination file name
    uint8_t buf[];                   // Buffer
};
}  // namespace data

namespace callback {

struct alignas(ALIGNMENT) GetUptimeCallback {
    std::array<char, sizeof("Uptime: 999h 99m 99s")> uptime;
};

enum class AckType {
    SUCCESS,
    ERROR_TGAPI_EXCEPTION,
    ERROR_INVALID_ARGUMENT,
    ERROR_COMMAND_IGNORED,
    ERROR_RUNTIME_ERROR,
    ERROR_CLIENT_ERROR,
};

struct alignas(ALIGNMENT) GenericAck {
    AckType result;  // result type
    // Error message, only valid when result type is not SUCCESS
    MessageStringArray error_msg{};

    // Create a new instance of the Generic Ack, error.
    explicit GenericAck(AckType result, const std::string& errorMsg)
        : result(result) {
        std::strncpy(this->error_msg.data(), errorMsg.c_str(), MAX_MSG_SIZE);
        this->error_msg[MAX_MSG_SIZE - 1] = '\0';
    }
    GenericAck() = default;

    // Create a new instance of the Generic Ack, success.
    static GenericAck ok() { return GenericAck(AckType::SUCCESS, "OK"); }
};

struct alignas(ALIGNMENT) UploadFileDryCallback : public GenericAck {
    data::UploadFileDry requestdata;
};

}  // namespace callback
}  // namespace TgBotSocket

// static asserts for compatibility check
// We make some asserts here to ensure that the size of the packet is expected
#define ASSERT_SIZE(DataType, DataSize)           \
    static_assert(sizeof(DataType) == (DataSize), \
                  "Size of " #DataType " has changed")

namespace TgBotSocket::data {
ASSERT_SIZE(WriteMsgToChatId, 264);
ASSERT_SIZE(ObserveChatId, 16);
ASSERT_SIZE(SendFileToChatId, 272);
ASSERT_SIZE(ObserveAllChats, 8);
ASSERT_SIZE(DeleteControllerById, 8);
ASSERT_SIZE(UploadFile, 552);
ASSERT_SIZE(DownloadFile, 512);
ASSERT_SIZE(PacketHeader, 32);
}  // namespace TgBotSocket::data

namespace TgBotSocket::callback {
ASSERT_SIZE(GetUptimeCallback, 24);
ASSERT_SIZE(GenericAck, 264);
ASSERT_SIZE(UploadFileDryCallback, 816);
}  // namespace TgBotSocket::callback
