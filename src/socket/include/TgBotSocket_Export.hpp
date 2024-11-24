#pragma once

// A header export for the TgBot's socket connection

#include <openssl/sha.h>
#include <zlib.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

#ifdef __TGBOT__
#include <Types.h>

#include <SharedMalloc.hpp>
#else
#include "../../include/SharedMalloc.hpp"
#include "../../include/Types.h"
#endif
#include "_TgBotSocketCommands.hpp"

template <typename T, size_t size>
inline bool arraycmp(const std::array<T, size>& lhs,
                     const std::array<T, size>& rhs) {
    if constexpr (std::is_same_v<T, char>) {
        return std::strncmp(lhs.data(), rhs.data(), size) == 0;
    }
    return std::memcmp(lhs.data(), rhs.data(), size) == 0;
}

template <size_t size>
inline void copyTo(std::array<char, size>& arr_in, const char* buf) {
    strncpy(arr_in.data(), buf, size);
}

template <size_t size, size_t size2>
    requires(size > size2)
inline void copyTo(std::array<char, size>& arr_in,
                   std::array<char, size2> buf) {
    copyTo(arr_in, buf.data());
}

#ifdef __cpp_concepts
template <size_t size, size_t size2>
    requires(size == size2)
inline void copyTo(std::array<char, size>& arr_in,
                   std::array<char, size2> buf) {
    copyTo(arr_in, buf.data());
    arr_in[size - 1] = '\0';
}
#endif

namespace TgBotSocket {

constexpr int MAX_PATH_SIZE = 256;
constexpr int MAX_MSG_SIZE = 256;
constexpr int ALIGNMENT = 8;

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
    /**
     * @brief Header for TgBotCommand Packets
     *
     * Header contains the magic value, command, and the size of the data
     */
    struct alignas(ALIGNMENT) Header {
        using length_type = uint64_t;
        constexpr static int64_t MAGIC_VALUE_BASE = 0xDEADFACE;
        // Version number, to be increased on breaking changes
        // 1: Initial version
        // 2: Added crc32 checks to packet data
        // 3: Uploadfile has a sha256sum check, std::array conversions
        // 4: Move CMD_UPLOAD_FILE_DRY to internal namespace
        // 5: Use the packed attribute for structs
        // 6: Make CMD_UPLOAD_FILE_DRY_CALLBACK return sperate callback, and add
        // srcpath to UploadFile
        // 7: Remove packed attribute, align all as 8 bytes
        // 8: change checksum to uint64_t
        // 9: Remove padding objects
        constexpr static int DATA_VERSION = 9;
        constexpr static int64_t MAGIC_VALUE = MAGIC_VALUE_BASE + DATA_VERSION;

        int64_t magic = MAGIC_VALUE;  ///< Magic value to verify the packet
        Command cmd{};                ///< Command to be executed
        ///< Size of the data in the packet
        length_type data_size{};
        ///< Checksum of the packet data
        uint64_t checksum{};
    };
    static_assert(offsetof(Header, magic) == 0);

    Header header{};
    SharedMalloc data;

    explicit Packet(Header::length_type length) : data(length) {
        header.magic = Header::MAGIC_VALUE;
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
    explicit Packet(Command cmd, T in_data, Header::length_type size)
        : data(size) {
        static_assert(std::is_pointer_v<T>,
                      "This constructor should not be used with non pointer");
        header.cmd = cmd;
        header.magic = Header::MAGIC_VALUE;
        header.data_size = size;
        data.assignFrom(in_data, header.data_size);
        header.checksum = crc32_function(data.get(), header.data_size);
    }

    static uLong crc32_function(const void* data, const size_t data_size) {
        uLong crc = crc32(0L, Z_NULL, 0);  // Initial value
        crc32(crc, static_cast<const Bytef*>(data), data_size);
        return crc;
    }

    static uLong crc32_function(const SharedMalloc& data) {
        return crc32_function(data.get(), data.size());
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

// Cannot use DELETE, as it conflicts with winnt.h
enum class CtrlSpamBlock {
    OFF,             // Disabled
    LOGGING_ONLY,    // Logging only, not taking action
    PURGE,           // Enabled, does delete but doesn't mute
    PURGE_AND_MUTE,  // Enabled, deletes and mutes
    MAX,
};

struct alignas(ALIGNMENT) WriteMsgToChatId {
    ChatId chat;                 // destination chatid
    MessageStringArray message;  // Msg to send
};

struct alignas(ALIGNMENT) ObserveChatId {
    ChatId chat;
    bool observe;  // new state for given ChatId,
                   // true/false - Start/Stop observing
};

struct alignas(ALIGNMENT) SendFileToChatId {
    ChatId chat;               // Destination ChatId
    FileType fileType;         // File type for file
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
    PathStringArray destfilepath{};  // Destination file name
    PathStringArray srcfilepath{};  // Source file name (This is not used on the
                                    // remote, used if dry=true)
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

        bool operator==(const Options& other) const = default;
    } options;

    bool operator==(const UploadFileDry& other) const = default;
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
        copyTo(error_msg, errorMsg.c_str());
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
ASSERT_SIZE(Packet::Header, 32);
}  // namespace TgBotSocket::data

namespace TgBotSocket::callback {
ASSERT_SIZE(GetUptimeCallback, 24);
ASSERT_SIZE(GenericAck, 264);
ASSERT_SIZE(UploadFileDryCallback, 816);
}  // namespace TgBotSocket::callback
