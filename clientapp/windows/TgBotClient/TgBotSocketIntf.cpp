
#pragma comment(lib, "ws2_32")

#define _CRT_SECURE_NO_WARNINGS
#include "TgBotSocketIntf.h"

#include <WS2tcpip.h>
#include <WinSock2.h>
#include <absl/log/log.h>
#include <zlib.h>

#include <filesystem>
#include <functional>
#include <iomanip>
#include <memory>
#include <optional>
#include <stdexcept>

using PktHeader = TgBotCommandPacketHeader;
using UniqueMalloc = std::unique_ptr<void, decltype(&free)>;

template <typename DataStruct,
          typename Deleter = std::default_delete<DataStruct>>
struct TgBotCommandContext {
    PktHeader header;
    std::unique_ptr<DataStruct, Deleter> data;

    TgBotCommandContext() = default;
    TgBotCommandContext(void *_data, Deleter deleter) : data(_data, deleter) {}
};

struct FileData {
    void *data;
    PktHeader::length_type len;
};

bool fileData_tofile(const void *ptr, size_t len) {
    const auto *data = static_cast<const char *>(ptr);
    UploadFile destfilepath{};
    FILE *file = nullptr;
    size_t ret = 0;
    size_t file_size = len - sizeof(UploadFile);

    LOG(INFO) << "This buffer has a size of " << len << " bytes";
    LOG(INFO) << "Which is " << file_size << " bytes excluding the header";

    strncpy(destfilepath, data, sizeof(UploadFile) - 1);
    if ((file = fopen(destfilepath, "wb")) == nullptr) {
        LOG(ERROR) << "Failed to open file: " << destfilepath;
        return false;
    }
    ret = fwrite(data + sizeof(UploadFile), file_size, 1, file);
    if (ret != 1) {
        LOG(ERROR) << "Failed to write to file: " << destfilepath << " (Wrote "
                   << ret << " bytes)";
        fclose(file);
        return false;
    }
    fclose(file);
    return true;
}

std::optional<FileData> fileData_fromFile(const std::string_view filename,
                                          const std::string_view destfilepath) {
    constexpr size_t datahdr_size = sizeof(UploadFile);
    size_t size = 0;
    size_t total_size = 0;
    char *buf = nullptr;
    FILE *fp = nullptr;
    std::optional<FileData> pkt;

    fp = fopen(filename.data(), "rb");
    if (fp != nullptr) {
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        total_size = size + datahdr_size;
        LOG(INFO) << "Sending file " << filename.data();
        buf = static_cast<char *>(malloc(total_size));
        LOG(INFO) << "mem-alloc buffer of size " << total_size << " bytes";
        // Copy data header to the beginning of the buffer.
        if (buf != nullptr) {
            strncpy(buf, destfilepath.data(), datahdr_size);
            char *moved_buf = buf + datahdr_size;
            fread(moved_buf, 1, size, fp);
            pkt = FileData{buf, total_size};
        }
        fclose(fp);
    } else {
        LOG(ERROR) << "Failed to open file " << filename.data();
    }
    return pkt;
}

static std::string WSALastErrStr() {
    char *s = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                       FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, WSAGetLastError(),
                   MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (LPSTR)&s, 0,
                   nullptr);
    std::string ret(s);
    LocalFree(s);
    return ret;
}

class TgBotSocketNative {
   public:
    using callback_data_handler_f =
        std::function<bool(const void *, PktHeader ::length_type)>;
    template <typename DataStruct, typename Deleter>
    bool sendContext(TgBotCommandContext<DataStruct, Deleter> &context,
                     callback_data_handler_f fn) const {
        bool ret = false;
        WSADATA __data;

        if (WSAStartup(kWSAVersion, &__data) != 0) {
            LOG(ERROR) << "Failed to WSAStartup";
            return false;
        }

        switch (config.mode) {
            case SocketConfig::Mode::USE_IPV4:
                ret = sendContextCommon<AF_INET, sockaddr_in>(context, fn);
                break;
            case SocketConfig::Mode::USE_IPV6:
                ret = sendContextCommon<AF_INET6, sockaddr_in6>(context, fn);
                break;
            default:
                LOG(ERROR) << "Unknown mode: " << static_cast<int>(config.mode);
                break;
        }
        WSACleanup();
        return ret;
    }

    void setSocketConfig(SocketConfig config_in) {
        config = std::move(config_in);
    }

    static std::shared_ptr<TgBotSocketNative> getInstance() {
        static auto instance =
            std::make_shared<TgBotSocketNative>(TgBotSocketNative());
        return instance;
    }

   private:
    constexpr static int kSocketPort = 50000;
    constexpr static int kRecvFlags = MSG_WAITALL;
    constexpr static DWORD kWSAVersion = MAKEWORD(2, 2);
    SocketConfig config;

    TgBotSocketNative() = default;

    static void __cdecl closeFd(SOCKET *fd) { closesocket(*fd); }
    static void LogWSAErr(const std::string_view message) {
        LOG(ERROR) << "Failed to " << message << ": " << WSALastErrStr();
    }

    template <int af, typename SockAddr, typename DataStruct, typename Deleter>
    bool sendContextCommon(TgBotCommandContext<DataStruct, Deleter> &context,
                           callback_data_handler_f callback) const {
        SockAddr addr{};
        socklen_t len = sizeof(SockAddr);
        SOCKET sockfd{};
        int ret{};

        LOG(INFO) << "Prepare to send CommandContext";

        sockfd = socket(af, SOCK_STREAM, 0);
        if (sockfd == INVALID_SOCKET) {
            LogWSAErr("create socket");
            return false;
        }

        auto sockFdCloser = std::unique_ptr<SOCKET, decltype(&closeFd)>(
            &sockfd, &TgBotSocketNative::closeFd);

        LOG(INFO) << "Using IP: " << std::quoted(config.address, '\'')
                  << ", Port: " << kSocketPort << " with af: " << af;
        setupSockAddress(&addr);

        // Calculate CRC32
        uLong crc = crc32(0L, Z_NULL, 0);  // Initial value
        crc = crc32(crc, reinterpret_cast<Bytef *>(context.data.get()),
                    context.header.data_size);
        context.header.checksum = crc;

        if (connect(sockfd, reinterpret_cast<sockaddr *>(&addr), len) != 0) {
            LogWSAErr("connect to server");
            return false;
        }
        LOG(INFO) << "Connected to server";
        ret = send(sockfd, reinterpret_cast<const char *>(&context.header),
                   sizeof(PktHeader), 0);
        if (ret < 0) {
            LogWSAErr("send packet header");
            return false;
        } else {
            DLOG(INFO) << "Sent header packet with cmd " << context.header.cmd
                       << ", " << ret << " bytes";
        }
        ret = send(sockfd, reinterpret_cast<const char *>(context.data.get()),
                   context.header.data_size, 0);
        if (ret < 0) {
            LogWSAErr("send packet data");
            return false;
        } else {
            DLOG(INFO) << "Sent data packet, " << ret << " bytes";
        }
        LOG(INFO) << "Done sending data";
        LOG(INFO) << "Now reading callback";

        PktHeader header;
        ret = recv(sockfd, reinterpret_cast<char *>(&header), sizeof(header),
                   kRecvFlags);
        if (ret < 0) {
            LogWSAErr("read callback header");
            return false;
        }
        if (header.magic != PktHeader::MAGIC_VALUE) {
            LOG(ERROR) << "Failed to validate magic value of callback header";
            return false;
        }
        UniqueMalloc data(malloc(header.data_size), &free);
        if (data == nullptr) {
            LOG(ERROR) << "Failed to alloc data for callback header";
            return false;
        }
        ret = recv(sockfd, static_cast<char *>(data.get()), header.data_size,
                   kRecvFlags);
        if (ret < 0) {
            LogWSAErr("recv callback data");
            return false;
        }
        switch (header.cmd) {
            case CMD_GET_UPTIME_CALLBACK:
            case CMD_DOWNLOAD_FILE_CALLBACK:
                if (!callback(data.get(), header.data_size)) {
                    LOG(ERROR) << "Failed to execute callback";
                    return false;
                }
                break;
            default: {
                GenericAck AckData{};
                bool success{};

                memcpy(&AckData, data.get(), sizeof(AckData));
                success = AckData.result == AckType::SUCCESS;
                LOG(INFO) << "Command ACK: " << std::boolalpha << success;
                if (!success) {
                    LOG(ERROR) << "Reason: " << AckData.error_msg;
                }
                // invoke the callback
                if (!callback(data.get(), header.data_size)) {
                    LOG(ERROR) << "Failed to execute callback";
                    return false;
                }
                return success;
            }
        }
        return true;
    }

    template <typename SockAddr>
    void setupSockAddress(SockAddr *addr) const = delete;
    template <>
    [[maybe_unused]] void setupSockAddress(sockaddr_in *addr) const {
        addr->sin_family = AF_INET;
        inet_pton(AF_INET, config.address.c_str(), &addr->sin_addr);
        addr->sin_port = htons(kSocketPort);
    }
    template <>
    [[maybe_unused]] void setupSockAddress(sockaddr_in6 *addr) const {
        addr->sin6_family = AF_INET6;
        inet_pton(AF_INET6, config.address.c_str(), &addr->sin6_addr);
        addr->sin6_port = htons(kSocketPort);
    }
};

using callback_t = TgBotSocketNative::callback_data_handler_f;

template <TgBotCommand cmd, typename DataStruct>
TgBotCommandContext<DataStruct> createContext(const DataStruct &data) {
    TgBotCommandContext<DataStruct> context;
    context.header.cmd = cmd;
    context.header.data_size = sizeof(DataStruct);
    context.data = std::make_unique<DataStruct>(data);
    return context;
}

template <TgBotCommand cmd, typename DataType>
bool trySendContext(DataType data, callback_t callback) {
    auto context = createContext<cmd>(data);
    return trySendContext(context, callback);
}

template <typename DataType, typename Deleter>
bool trySendContext(TgBotCommandContext<DataType, Deleter> &context,
                    callback_t callback) {
    const auto socket = TgBotSocketNative::getInstance();
    return socket->sendContext(context, callback);
}

std::string getUptime() {
    std::string result;
    bool dummy = true;
    trySendContext<CMD_GET_UPTIME>(
        dummy, [&result](const void *data, PktHeader::length_type sz) {
            const auto *uptime = static_cast<const char *>(data);
            result = std::string(uptime, sz);
            return true;
        });
    return result;
}

bool sendMessageToChat(ChatId id, std::string message,
                       GenericAckCallback callback) {
    WriteMsgToChatId data{};
    strncpy(data.msg, message.c_str(), MAX_MSG_SIZE - 1);
    data.to = id;
    return trySendContext<CMD_WRITE_MSG_TO_CHAT_ID>(
        data, [callback](const void *data, PktHeader::length_type sz) {
            const auto *result = static_cast<const GenericAck *>(data);
            callback(result);
            return true;
        });
}

bool sendFileToChat(ChatId id, std::filesystem::path filepath,
                    GenericAckCallback callback) {
    SendFileToChatId data{};
    GenericAck resultAck{};
    bool ret = false;
    auto fData = fileData_fromFile(filepath.string().c_str(),
                                   filepath.filename().string().c_str());
    if (!fData) {
        resultAck.result = AckType::ERROR_RUNTIME_ERROR;
        strncpy(resultAck.error_msg, "Client error: Failed to prepare file",
                sizeof(resultAck.error_msg));
        callback(&resultAck);
        return false;
    }
    TgBotCommandContext<void, decltype(&free)> context(fData->data, &free);
    context.header.cmd = CMD_UPLOAD_FILE;
    context.header.data_size = fData->len;
    // TODO: use the msg by server here too, when it failed
    ret = trySendContext(
        context,
        [](const void *data, PktHeader::length_type sz) { return true; });
    if (!ret) {
        resultAck.result = AckType::ERROR_RUNTIME_ERROR;
        strncpy(resultAck.error_msg, "Client error: Failed to upload file",
                sizeof(resultAck.error_msg));
        callback(&resultAck);
        return false;
    }

    strncpy(data.filepath, filepath.filename().string().c_str(),
            MAX_PATH_SIZE - 1);
    data.id = id;
    data.type = TYPE_DOCUMENT;
    ret = trySendContext<CMD_SEND_FILE_TO_CHAT_ID>(
        data, [callback](const void *data, PktHeader::length_type sz) {
            const auto *result = static_cast<const GenericAck *>(data);
            callback(result);
            return true;
        });
    return ret;
}

void setSocketConfig(SocketConfig config) {
    TgBotSocketNative::getInstance()->setSocketConfig(config);
    LOG(INFO) << "New config: IPAddress: " << config.address;
    LOG(INFO) << "New config: AF: " << config.ModetoStr();
}