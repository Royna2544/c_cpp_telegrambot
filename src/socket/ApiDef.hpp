#pragma once

// A header export for the TgBot's socket connection
#include <api/typedefs.h>
#include <openssl/evp.h>
#include <openssl/sha.h>

#include <SharedMalloc.hpp>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <fmt/format.h>

template <typename T, size_t size>
inline bool arraycmp(const std::array<T, size>& lhs,
                     const std::array<T, size>& rhs) {
    if constexpr (std::is_same_v<T, char>) {
        return std::strncmp(lhs.data(), rhs.data(), size) == 0;
    }
    return std::memcmp(lhs.data(), rhs.data(), size) == 0;
}

template <size_t size>
inline void copyTo(std::array<char, size>& arr_in, const std::string_view buf) {
    strncpy(arr_in.data(), buf.data(), std::min(size - 1, buf.size()));
    arr_in[size - 1] = '\0';
}

template <size_t size, size_t size2>
inline void copyTo(std::array<char, size>& arr_in,
                   std::array<char, size2> buf) {
    static_assert(size >= size2,
                  "Destination must be same or bigger than source size");
    copyTo(arr_in, buf.data());
    arr_in[size - 1] = '\0';
}

// Helper to safely extract underlying type ONLY if it's an enum
template <typename T, bool IsEnum = std::is_enum_v<T>>
struct SafeUnderlying {
    using type = T;
};

template <typename T>
struct SafeUnderlying<T, true> {
    using type = std::underlying_type_t<T>;
};

template <typename Underlying>
struct ByteHelper {
    Underlying value;
    using type = Underlying;

    // Helper to get the integer type (works for ints and enums)
    using IntType = typename SafeUnderlying<Underlying>::type;

    constexpr operator Underlying() const {
        if constexpr (std::endian::native == std::endian::little)
            // Cast to integer, swap, then cast back
            return static_cast<Underlying>(
                std::byteswap(static_cast<IntType>(value)));
        else
            return value;
    }

    constexpr operator IntType() const 
        requires (!std::is_same_v<Underlying, IntType>) {
        if constexpr (std::endian::native == std::endian::little)
            // Cast to integer, swap
            return std::byteswap(static_cast<IntType>(value));
        else
            return static_cast<IntType>(value);
    }

    constexpr ByteHelper(Underlying v) {
        if constexpr (std::endian::native == std::endian::little)
            value =
                static_cast<Underlying>(std::byteswap(static_cast<IntType>(v)));
        else
            value = v;
    }

    ByteHelper() = default;

    // Allow assignment operator for convenience
    ByteHelper& operator=(Underlying v) {
        *this = ByteHelper(v);
        return *this;
    }
};

template <typename T>
struct fmt::formatter<ByteHelper<T>> : fmt::formatter<T> {
    // 'parse' is inherited from base, so we only need to implement 'format'

    template <typename FormatContext>
    auto format(const ByteHelper<T>& wrapper, FormatContext& ctx) const {
        // static_cast invokes the conversion operator we defined earlier
        // which handles the endian swap automatically.
        return fmt::formatter<T>::format(static_cast<T>(wrapper), ctx);
    }
};

namespace TgBotSocket {

enum class Command : std::int32_t {
    CMD_INVALID,
    CMD_WRITE_MSG_TO_CHAT_ID,
    CMD_CTRL_SPAMBLOCK,
    CMD_OBSERVE_CHAT_ID,
    CMD_SEND_FILE_TO_CHAT_ID,
    CMD_OBSERVE_ALL_CHATS,
    CMD_GET_UPTIME,
    CMD_TRANSFER_FILE,  // TODO: Remove
    CMD_TRANSFER_FILE_REQUEST,
    CMD_TRANSFER_FILE_BEGIN,
    CMD_TRANSFER_FILE_CHUNK,
    CMD_TRANSFER_FILE_CHUNK_RESPONSE,
    CMD_TRANSFER_FILE_END,
    CMD_CLIENT_MAX = CMD_TRANSFER_FILE_END,

    // Below are internal commands
    CMD_SERVER_INTERNAL_START = 100,
    CMD_GET_UPTIME_CALLBACK = CMD_SERVER_INTERNAL_START,
    CMD_GENERIC_ACK,
    CMD_OPEN_SESSION,
    CMD_OPEN_SESSION_ACK,
    CMD_CLOSE_SESSION,
    CMD_MAX,
};

enum class PayloadType : int32_t { Binary, Json };

constexpr int MAX_PATH_SIZE = 256;
constexpr int MAX_MSG_SIZE = 256;
constexpr int ALIGNMENT = 8;

/**
 * @brief Packet for sending commands to the server
 *
 * This packet is used to send commands to the server.
 * It contains a header, which contains the magic value, the command, and the
 * size of the data. The data is then followed by the actual data.
 */
struct alignas(ALIGNMENT) Packet {
    using hmac_type = std::array<uint8_t, SHA256_DIGEST_LENGTH>;

    /**
     * @brief Header for TgBotCommand Packets
     *
     * Header contains the magic value, command, and the size of the data
     */
    struct alignas(ALIGNMENT) Header {
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
        // 10: Alignments fixing for Python compliance, add INVALID_CMD at 0
        // 11: Remove CMD_DELETE_CONTROLLER_BY_ID, add payload type to header
        // 12: Use OpenSSL's HMAC and AES-GCM algorithm encryption with nounces.
        // Also use CMD_OPEN_SESSION CMD_CLOSE_SESSION for session based
        // encryption
        // 13: HMAC covers header as well, move hmac to end of data
        // (Not Implemented!) CMD_TRANSFER_FILE with sessions and chunk-base
        constexpr static int DATA_VERSION = 13;
        constexpr static int64_t MAGIC_VALUE = MAGIC_VALUE_BASE + DATA_VERSION;

        using length_type = uint32_t;
        using nounce_type = uint64_t;

        // Using AES-GCM
        constexpr static int IV_LENGTH = 12;
        constexpr static int TAG_LENGTH = 16;
        constexpr static int SESSION_TOKEN_LENGTH = 32;

        using session_token_type = std::array<char, SESSION_TOKEN_LENGTH>;
        using init_vector_type = std::array<uint8_t, IV_LENGTH>;

        ByteHelper<int64_t> magic =  MAGIC_VALUE;  ///< Magic value to verify the packet
        ByteHelper<Command> cmd;        ///< Command to be executed
        ByteHelper<PayloadType> data_type;   ///< Type of payload in the packet
        ByteHelper<length_type> data_size;    ///< Size of the data in the packet
        session_token_type session_token;  ///< Session token
        ByteHelper<nounce_type> nonce;   ///< Nonce (Epoch timestamp is used)
        init_vector_type init_vector;    ///< Initialization vector for AES-GCM
    } header{};
    SharedMalloc data;
    hmac_type hmac{};
};

using PathStringArray = std::array<char, MAX_PATH_SIZE>;
using MessageStringArray = std::array<char, MAX_MSG_SIZE>;
using SHA256StringArray = std::array<uint8_t, SHA256_DIGEST_LENGTH>;

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

/**
 * CMD_WRITE_MESSAGE_TO_CHAT's data structure
 *
 * JSON schema:
 * {
 *   "chat": int64,
 *   "message": string
 * }
 */
struct alignas(ALIGNMENT) WriteMsgToChatId {
    ByteHelper<ChatId> chat;     // destination chatid
    MessageStringArray message;  // Msg to send
};

/**
 * CMD_OBSERVE_CHAT_ID's data structure
 *
 * JSON schema:
 * {
 *   "chat": int64,
 *   "observe": True/False
 * }
 */
struct alignas(ALIGNMENT) ObserveChatId {
    ByteHelper<ChatId> chat;
    bool observe;  // new state for given ChatId,
                   // true/false - Start/Stop observing
};

/**
 * CMD_SEND_FILE_TO_CHAT's data structure
 *
 * JSON schema:
 * {
 *   "chat": int64,
 *   "fileType": TgBotSocket::data::FileType value as int (TODO: Make it string
 * representation), "filePath": string
 * }
 */

struct alignas(ALIGNMENT) SendFileToChatId {
    ByteHelper<ChatId> chat;                      // Destination ChatId
    ByteHelper<FileType> fileType;                  // File type for file
    alignas(ALIGNMENT) PathStringArray filePath;  // Path to file (local)
};

/**
 * CMD_OBSERVE_ALL_CHATS data structure.
 *
 * JSON schema:
 * {
 *   "observe": True/False
 * }
 */
struct alignas(ALIGNMENT) ObserveAllChats {
    bool observe;  // new state for all chats,
                   // true/false - Start/Stop observing
};

// This border byte is used to distinguish JSON meta and file data
constexpr uint8_t JSON_BYTE_BORDER = 0xFF;

/**
 * CMD_UPLOAD_FILE/CMD_DOWNLOAD_FILE data structure.
 *
 * JSON schema:
 *
 * {
 *   "destfilepath": string,
 *   "srcfilepath": string,
 *   "hash": string (hexadecimal representation, Optional if options.hash_ignore
 * is true), "options": (Optional if no options are specified) { "overwrite":
 * bool, (Optional, Default=False) "hash_ignore": bool, (Optional,
 * Default=False)
 *    }
 * }
 * Note that JSON cannot include bytes. So we use a border at the end of the
 * JSON metadata. Example:
 *
 * {
 *   "destfilepath": "/path/to/destination/file.txt",
 *   "srcfilepath": "/path/to/source/file.txt",
 *   "hash": "67452400d478e8226a73a440e082f7a746e696c6c65",
 *   "options": {
 *     "overwrite": true,
 *     "hash_ignore": true
 *   }
 *   ==BORDER==
 *   <file bytes>
 */
struct alignas(ALIGNMENT) FileTransferMeta {
    PathStringArray srcfilepath{};  // Source file name (This is not used on the
                                    // remote, used if dry=true)
    PathStringArray destfilepath{};   // Destination file name
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
        // If this is false, the file must be followed after this struct
        bool dry_run = false;

        constexpr bool operator==(const Options& other) const {
            return overwrite == other.overwrite &&
                   hash_ignore == other.hash_ignore && dry_run == other.dry_run;
        }
    } options;

    bool operator==(const FileTransferMeta& other) const {
        return arraycmp(destfilepath, other.destfilepath) &&
               arraycmp(srcfilepath, other.srcfilepath) &&
               arraycmp(sha256_hash, sha256_hash) && options == other.options;
    }
};

}  // namespace data

namespace callback {

/**
 * CMD_GET_UPTIME's callback data structure
 *
 * JSON schema:
 * {
 *   "start_time": timestamp
 *   "current_time": timestamp
 *   "uptime": string (format: "Uptime: 999h 99m 99s")
 * }
 */
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

/**
 * GenericAck's JSON schema:
 * {
 *   "result": True|False
 *   "__comment__": "Below two fields are optional"
 *   "error_type":
 * "TGAPI_EXCEPTION"|"INVALID_ARGUMENT"|"COMMAND_IGNORED"|"RUNTIME_ERROR"|"CLIENT_ERROR",
 *   "error_msg": "Error message"
 * }
 */

struct alignas(ALIGNMENT) GenericAck {
    ByteHelper<AckType> result{};  // result type
    // Error message, only valid when result type is not SUCCESS
    alignas(ALIGNMENT) MessageStringArray error_msg{};

    // Create a new instance of the Generic Ack, error.
    explicit GenericAck(AckType result, const std::string& errorMsg)
        : result(result) {
        copyTo(error_msg, errorMsg);
    }
    GenericAck() = default;

    // Create a new instance of the Generic Ack, success.
    static GenericAck ok() { return GenericAck(AckType::SUCCESS, "OK"); }
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
ASSERT_SIZE(FileTransferMeta, 552);
ASSERT_SIZE(Packet::Header, 80);
}  // namespace TgBotSocket::data

namespace TgBotSocket::callback {
ASSERT_SIZE(GetUptimeCallback, 24);
ASSERT_SIZE(GenericAck, 264);
}  // namespace TgBotSocket::callback
