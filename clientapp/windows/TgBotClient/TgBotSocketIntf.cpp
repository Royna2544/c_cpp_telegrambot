
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

#include "../../../src/socket/interface/bot/TgBotSocketFileHelperNew.cpp"

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
        std::function<bool(const void *, PacketHeader::length_type)>;

    bool sendContext(Packet &pkt, callback_data_handler_f fn) const {
        bool ret = false;
        WSADATA __data;

        if (WSAStartup(kWSAVersion, &__data) != 0) {
            LOG(ERROR) << "Failed to WSAStartup";
            return false;
        }

        switch (config.mode) {
            case SocketConfig::Mode::USE_IPV4:
                ret = sendContextCommon<AF_INET, sockaddr_in>(pkt, fn);
                break;
            case SocketConfig::Mode::USE_IPV6:
                ret = sendContextCommon<AF_INET6, sockaddr_in6>(pkt, fn);
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
    constexpr static int kRecvFlags = MSG_WAITALL;
    constexpr static DWORD kWSAVersion = MAKEWORD(2, 2);
    SocketConfig config;

    TgBotSocketNative() = default;

    static void __cdecl closeFd(SOCKET *fd) { closesocket(*fd); }
    static void LogWSAErr(const std::string_view message) {
        LOG(ERROR) << "Failed to " << message << ": " << WSALastErrStr();
    }

    template <int af, typename SockAddr>
    bool sendContextCommon(Packet &context,
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
                  << ", Port: " << config.port << " with af: " << af;
        setupSockAddress(&addr);

        // Calculate CRC32
        context.header.checksum = Packet::crc32_function(context.data);

        if (connect(sockfd, reinterpret_cast<sockaddr *>(&addr), len) != 0) {
            LogWSAErr("connect to server");
            return false;
        }
        LOG(INFO) << "Connected to server";
        ret = send(sockfd, reinterpret_cast<const char *>(&context.header),
                   sizeof(PacketHeader), 0);
        if (ret < 0) {
            LogWSAErr("send packet header");
            return false;
        } else {
            DLOG(INFO) << "Sent header packet with cmd "
                       << static_cast<int>(context.header.cmd) << ", " << ret
                       << " bytes";
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

        PacketHeader header;
        ret = recv(sockfd, reinterpret_cast<char *>(&header), sizeof(header),
                   kRecvFlags);
        if (ret < 0) {
            LogWSAErr("read callback header");
            return false;
        }
        if (header.magic != PacketHeader::MAGIC_VALUE) {
            LOG(ERROR) << "Failed to validate magic value of callback header";
            return false;
        }
        SharedMalloc data(header.data_size);
        if (data.get() == nullptr) {
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
            case Command::CMD_GET_UPTIME_CALLBACK:
            case Command::CMD_DOWNLOAD_FILE_CALLBACK:
                if (!callback(data.get(), header.data_size)) {
                    LOG(ERROR) << "Failed to execute callback";
                    return false;
                } else {
                    LOG(INFO) << "Callback handled: OK";
                }
                break;
            case Command::CMD_UPLOAD_FILE_DRY_CALLBACK:
            default: {
                GenericAck AckData{};
                bool success{};

                memcpy(&AckData, data.get(), sizeof(AckData));
                success = AckData.result == AckType::SUCCESS;
                LOG(INFO) << "Command ACK: " << std::boolalpha << success;
                if (!success) {
                    LOG(ERROR) << "Reason: " << AckData.error_msg.data();
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
        addr->sin_port = htons(config.port);
    }
    template <>
    [[maybe_unused]] void setupSockAddress(sockaddr_in6 *addr) const {
        addr->sin6_family = AF_INET6;
        inet_pton(AF_INET6, config.address.c_str(), &addr->sin6_addr);
        addr->sin6_port = htons(config.port);
    }
};

using callback_t = TgBotSocketNative::callback_data_handler_f;

template <Command cmd, typename T>
bool trySendContext(T data, callback_t callback) {
    const auto socket = TgBotSocketNative::getInstance();
    Packet pkt(cmd, data);
    return socket->sendContext(pkt, callback);
}

std::string getUptime() {
    std::string result;
    bool dummy = true;
    trySendContext<Command::CMD_GET_UPTIME>(
        dummy, [&result](const void *data, PacketHeader::length_type sz) {
            const auto *uptime = static_cast<const char *>(data);
            result = std::string(uptime, sz);
            return true;
        });
    return result;
}

bool sendMessageToChat(ChatId id, std::string message,
                       GenericAckCallback callback) {
    WriteMsgToChatId data{};
    copyTo(data.message, message.c_str());
    data.chat = id;
    return trySendContext<Command::CMD_WRITE_MSG_TO_CHAT_ID>(
        data, [callback](const void *data, PacketHeader::length_type sz) {
            const auto *result = static_cast<const GenericAck *>(data);
            callback(result);
            return true;
        });
}

bool sendFileToChat(ChatId id, std::filesystem::path filepath, FileType type,
                    GenericAckCallback callback) {
    SendFileToChatId data{};
    GenericAck resultAck{};
    bool ret = false;
    RealFS fs;
    SocketFile2DataHelper helper{&fs};
    SocketFile2DataHelper::DataFromFileParam param;

    param.filepath = filepath;
    param.destfilepath = filepath.filename();
    auto fData =
        helper.DataFromFile<SocketFile2DataHelper::Pass::UPLOAD_FILE_DRY>(
            param);
    if (!fData) {
        resultAck.result = AckType::ERROR_RUNTIME_ERROR;
        copyTo(resultAck.error_msg,
               "Client error: Failed to prepare file (dry)");
        callback(&resultAck);
        return false;
    }

    ret = TgBotSocketNative::getInstance()->sendContext(
        fData.value(),
        [&resultAck](const void *data, PacketHeader::length_type sz) {
            resultAck = *static_cast<const GenericAck *>(data);
            return true;
        });
    if (!ret) {
        callback(&resultAck);
        return false;
    }

    fData =
        helper.DataFromFile<
        SocketFile2DataHelper::Pass::UPLOAD_FILE>(
            param);
    ret = TgBotSocketNative::getInstance()->sendContext(
        fData.value(),
        [&resultAck](const void *data, PacketHeader::length_type sz) {
            resultAck = *static_cast<const GenericAck *>(data);
            return true;
        });
    if (!ret) {
        callback(&resultAck);
        return false;
    }

    copyTo(data.filePath, filepath.filename().string().c_str());
    data.chat = id;
    data.fileType = type;
    ret = trySendContext<Command::CMD_SEND_FILE_TO_CHAT_ID>(
        data, [callback](const void *data, PacketHeader::length_type sz) {
            const auto *result = static_cast<const GenericAck *>(data);
            callback(result);
            return true;
        });
    return ret;
}

bool downloadFile(std::filesystem::path localDest,
                  std::filesystem::path remoteSrc,
                  GenericAckCallback callback) {
    GenericAck resultAck{};

    data::DownloadFile data{};
    copyTo(data.filepath, remoteSrc.string().c_str());
    copyTo(data.destfilename, localDest.string().c_str());
    auto pkt = Packet(Command::CMD_DOWNLOAD_FILE, data);
    return TgBotSocketNative::getInstance()->sendContext(
        pkt, [](const void *data, PacketHeader::length_type sz) {
            RealFS fs;
            SocketFile2DataHelper helper{&fs};
            return helper.DataToFile<SocketFile2DataHelper::Pass::DOWNLOAD_FILE>(
                data, sz);
        });
}

void setSocketConfig(SocketConfig config) {
    TgBotSocketNative::getInstance()->setSocketConfig(config);
    LOG(INFO) << "New config: IPAddress: " << config.address;
    LOG(INFO) << "New config: AF: " << config.ModetoStr();
    LOG(INFO) << "New config: Port num: " << config.port;
}