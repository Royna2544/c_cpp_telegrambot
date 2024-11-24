#include <absl/log/initialize.h>
#include <arpa/inet.h>
#include <absl/log/log.h>
#include <cerrno>
#include <jni.h>
#include <unistd.h>
#include <zlib.h>
#include <sys/poll.h>

#include <cstring>
#include <string>
#include <string_view>
#include <utility>

#include "JNIOnLoad.h"
#include "JavaCppConverter.hpp"
// Include the tgbot's exported header
#include "../../../../../../src/socket/interface/bot/TgBotSocketFileHelperNew.hpp"

using namespace TgBotSocket;
using namespace TgBotSocket::data;
using namespace TgBotSocket::callback;

struct SocketConfig {
    std::string address;
    enum : std::uint8_t { USE_IPV4, USE_IPV6 } mode {};
    int port {};

    int timeout_s = 5;
};

// Java class definitions
#define SOCKNATIVE_JAVACLS "com/royna/tgbotclient/SocketCommandNative"
#define SOCKNATIVE_CMDCB_JAVACLS SOCKNATIVE_JAVACLS "$ICommandStatusCallback"
#define SOCKNATIVE_DSTINFO_JAVACLS SOCKNATIVE_JAVACLS "$DestinationInfo"
#define SOCKNATIVE_DSTTYPE_JAVACLS SOCKNATIVE_JAVACLS "$DestinationType"
#define STRING_JAVACLS "java/lang/String"
#define OBJECT_JAVACLS "java/lang/Object"

class Callbacks {
   public:
    enum class Status {
        INVALID,
        CONNECTION_PREPARED,
        CONNECTED,
        HEADER_PACKET_SENT,
        DATA_PACKET_SENT,
        HEADER_PACKET_RECEIVED,
        DATA_PACKET_RECEIVED,
        PROCESSED_DATA,
        DONE,
    };

    Callbacks(JNIEnv *_env, jobject _callbackObj) : env(_env), callbackInstance(_callbackObj) {
        const auto callbackClass = env->FindClass(SOCKNATIVE_CMDCB_JAVACLS);
        methods.success = env->GetMethodID(callbackClass, "onSuccess",
                                           "(L" OBJECT_JAVACLS ";)V");
        methods.failure = env->GetMethodID(callbackClass, "onError", "(L" STRING_JAVACLS ";)V");
        methods.status = env->GetMethodID(callbackClass, "onStatusUpdate", "(I)V");
    }

    void success(jobject result) const {
        DLOG(INFO) << "Calling callback: " << __func__;
        env->CallVoidMethod(callbackInstance, methods.success, result);
    }

    void success(const std::string_view result) const {
        success(Convert<jstring>(env, result));
    }

    void failure(const std::string_view message) const {
        LOG(ERROR) << message;
        _failure(message);
    }

    void plog_failure(const std::string_view message) const {
        PLOG(ERROR) << message;
        _failure(message);
    }

    void status(const Status status) const {
#ifndef NDEBUG
#define STATUSLOG "[status_log] "
        switch (status) {
            case Status::CONNECTION_PREPARED:
                LOG(INFO) << STATUSLOG "Preparing connection";
                break;
            case Status::CONNECTED:
                LOG(INFO) << STATUSLOG "Connected";
                break;
            case Status::HEADER_PACKET_SENT:
                LOG(INFO) << STATUSLOG "Header packet sent";
                break;
            case Status::DATA_PACKET_SENT:
                LOG(INFO) << STATUSLOG "Data packet sent";
                break;
            case Status::HEADER_PACKET_RECEIVED:
                LOG(INFO) << STATUSLOG "Header packet received";
                break;
            case Status::DATA_PACKET_RECEIVED:
                LOG(INFO) << STATUSLOG "Data packet received";
                break;
            case Status::PROCESSED_DATA:
                LOG(INFO) << STATUSLOG "Processed result packet";
                break;
            case Status::DONE:
                LOG(INFO) << STATUSLOG "Finished";
                break;
            default:
                break;
        }
#endif
        env->CallVoidMethod(callbackInstance, methods.status, static_cast<int>(status));
    }

   private:
    inline void _failure(const std::string_view message) const {
        DLOG(INFO) << "Calling callback: " << __func__;
        env->CallVoidMethod(callbackInstance, methods.failure,
                            Convert<jstring>(env, message));
    }
    JNIEnv *env;
    jobject callbackInstance;
    struct {
        jmethodID success;
        jmethodID failure;
        jmethodID status;
    } methods{};
};

RealFS realfs;
SocketFile2DataHelper helper(&realfs);

class TgBotSocketNative {
   public:

    void sendContext(Packet &packet, JNIEnv *env, jobject callbackObj) const {
        sendContext(packet, std::make_unique<Callbacks>(env, callbackObj));
    }

    void sendContext(Packet &packet, std::unique_ptr<Callbacks> callbacks) const {
        switch (config.mode) {
            case SocketConfig::USE_IPV4:
                sendContextCommon<AF_INET, sockaddr_in>(packet, std::move(callbacks));
                break;
            case SocketConfig::USE_IPV6:
                sendContextCommon<AF_INET6, sockaddr_in6>(packet, std::move(callbacks));
                break;
            default:
                LOG(ERROR) << "Unknown mode: " << static_cast<int>(config.mode);
                break;
        }
    }

    static TgBotSocketNative* getInstance() {
        static auto instance = TgBotSocketNative();
        return &instance;
    }

    SocketConfig config{};
    UploadFile::Options uploadOptions{};
   private:
    constexpr static int kRecvFlags = MSG_WAITALL | MSG_NOSIGNAL;
    constexpr static int kSendFlags = MSG_NOSIGNAL;

    TgBotSocketNative() = default;

    // Partial template specialization for sockaddr_in/sockaddr_in6
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

    template <int af, typename SockAddr>
    void sendContextCommon(Packet &context, std::unique_ptr<Callbacks> callbacks) const {
        SockAddr addr{};
        socklen_t len = sizeof(SockAddr);
        int sockfd{};
        int ret{};

        LOG(INFO) << "Prepare to send CommandContext";
        sockfd = socket(af, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (sockfd < 0) {
            callbacks->plog_failure("Failed to create socket");
            return;
        }

        auto sockFdCloser = std::unique_ptr<int, void(*)(const int*)>(
            &sockfd, [](const int *fd) { close(*fd); });
        LOG(INFO) << "Using IP: " << std::quoted(config.address, '\'')
                       << ", Port: " << config.port;
        setupSockAddress(&addr);

        struct timeval timeout {.tv_sec = 5 };
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        // Calculate CRC32
        context.header.checksum = Packet::crc32_function(context.data);

        // Update status 1
        callbacks->status(Callbacks::Status::CONNECTION_PREPARED);

        if (connect(sockfd, reinterpret_cast<sockaddr *>(&addr), len) != 0) {
            if (errno != EINPROGRESS) {
                callbacks->plog_failure("Failed to initiate connect()");
                return;
            }
        }

        // Wait for the nonblocking socket's event...
        struct pollfd fds{};
        fds.events = POLLOUT;
        fds.fd = sockfd;
        ret = poll(&fds, 1, config.timeout_s * 1000);
        if (ret < 0) {
            callbacks->plog_failure("Failed to poll()");
            return;
        } else if (!(fds.revents & POLLOUT)) {
            callbacks->plog_failure("The server didn't respond");
            return;
        }

        // Get if it failed...
        int error = 0;
        socklen_t error_len = sizeof(error);
        ret = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &error_len);
        if (ret < 0) {
            callbacks->plog_failure("Failed to getsockopt()");
            return;
        }

        // If failed to connect, abort
        if (error != 0) {
            callbacks->plog_failure("Failed to connect to server");
            return;
        }

        callbacks->status(Callbacks::Status::CONNECTED);

        ret = fcntl(sockfd, F_GETFL);
        if (ret < 0) {
            callbacks->plog_failure("Failed to get socket fd flags");
            return;
        }
        if (!(ret & O_NONBLOCK)) {
            ret = fcntl(sockfd, F_SETFL, ret & ~O_NONBLOCK);
            if (ret < 0) {
                callbacks->plog_failure("Failed to set socket fd blocking");
                return;
            }
        }

        ret = send(sockfd, &context.header, sizeof(Packet::Header), kSendFlags);
        if (ret < 0) {
            callbacks->plog_failure("Failed to send packet header");
            return;
        } else {
            LOG(INFO) << "Sent header packet with cmd "
                           << static_cast<int>(context.header.cmd) << ", "
                           << ret << " bytes";
        }
        callbacks->status(Callbacks::Status::HEADER_PACKET_SENT);

        ret = send(sockfd, context.data.get(), context.header.data_size,
                   kSendFlags);
        if (ret < 0) {
            callbacks->plog_failure("Failed to send packet data");
            return;
        } else {
            LOG(INFO) << "Sent data packet, " << ret << " bytes";
        }
        callbacks->status(Callbacks::Status::DATA_PACKET_SENT);

        LOG(INFO) << "Now reading callback";

#define RETRY(exp) ({         \
    __typeof__(exp) _rc;                   \
    do {                                   \
        _rc = (exp);                       \
    } while (_rc == -1 && (errno == EINTR || errno == EAGAIN)); \
    _rc; })

        Packet::Header header;
        ret = RETRY(recv(sockfd, &header, sizeof(header), kRecvFlags));
        if (ret < 0) {
            callbacks->plog_failure("Failed to read callback header");
            return;
        }
        callbacks->status(Callbacks::Status::HEADER_PACKET_RECEIVED);

        if (header.magic != Packet::Header ::MAGIC_VALUE) {
            LOG(WARNING) << "Magic value offset: " << header.magic - Packet::Header::MAGIC_VALUE_BASE;
            callbacks->failure("Bad magic value of callback header");
            return;
        }
        DLOG(INFO) << "Allocating " << header.data_size << " bytes...";
        SharedMalloc data(header.data_size);
        if (data.get() == nullptr) {
            if (header.data_size == 0) {
                callbacks->failure("Server returned 0 bytes of data");
            } else {
                errno = ENOMEM;
                callbacks->plog_failure("Failed to alloc data for callback header");
            }
            return;
        }
        ret = RETRY(recv(sockfd, data.get(), header.data_size, kRecvFlags));
        if (ret < 0) {
            callbacks->plog_failure("Failed to read callback data");
            return;
        }
        callbacks->status(Callbacks::Status::DATA_PACKET_RECEIVED);

#define CHECK_RESULTDATA_SIZE(type, size) do {\
    if ((size) != sizeof(type)) {\
        LOG(ERROR) << "Received packet with invalid size: " << (size);\
        callbacks->failure("Invalid size for " #type);\
        return;\
    }} while(false)

        LOG(INFO) << "Command received: " << static_cast<int>(header.cmd);
        switch (header.cmd) {
            case Command::CMD_GET_UPTIME_CALLBACK:
                handleSpecificData<Command::CMD_GET_UPTIME_CALLBACK>(
                    std::move(callbacks), data.get(), header.data_size);
                break;
            case Command::CMD_DOWNLOAD_FILE_CALLBACK:
                handleSpecificData<Command::CMD_DOWNLOAD_FILE_CALLBACK>(
                        std::move(callbacks), data.get(), header.data_size);
                break;
            case Command::CMD_UPLOAD_FILE_DRY_CALLBACK:
                handleSpecificData<Command::CMD_UPLOAD_FILE_DRY_CALLBACK>(
                        std::move(callbacks), data.get(), header.data_size);
                break;
            default: {
                GenericAck AckData{};
                bool success{};
                CHECK_RESULTDATA_SIZE(GenericAck, header.data_size);
                memcpy(&AckData, data.get(), sizeof(AckData));
                success = AckData.result == AckType::SUCCESS;
                LOG(INFO) << "Command ACK: " << std::boolalpha << success;
                if (!success) {
                    LOG(ERROR) << "Reason: " << AckData.error_msg.data();
                    callbacks->plog_failure(AckData.error_msg.data());
                } else {
                    callbacks->success(nullptr);
                }
                break;
            }
        }
    }


    template <Command cmd>
    void handleSpecificData(std::unique_ptr<Callbacks> callbacks, const void *data,
                            Packet::Header::length_type len) const = delete;
    template <>
    void handleSpecificData<Command::CMD_DOWNLOAD_FILE_CALLBACK>(
            std::unique_ptr<Callbacks> callbacks, const void *data, Packet::Header::length_type len) const {
        bool rc =
            helper.DataToFile<SocketFile2DataHelper::Pass::DOWNLOAD_FILE>(
                data, len);
        if (rc) {
            callbacks->success(nullptr);
        } else {
            callbacks->failure("Failed to handle download file");
        }
    }
    template <>
    void handleSpecificData<Command::CMD_GET_UPTIME_CALLBACK>(
        std::unique_ptr<Callbacks> callbacks, const void *data,
        Packet::Header::length_type len) const {
        const auto *uptime = static_cast<const char *>(data);
        CHECK_RESULTDATA_SIZE(GetUptimeCallback, len);
        callbacks->success(uptime);
    }

    template <>
    void handleSpecificData<Command::CMD_UPLOAD_FILE_DRY_CALLBACK>(
            std::unique_ptr<Callbacks>  callbacks, const void *data,
        Packet::Header::length_type len) const {
        CHECK_RESULTDATA_SIZE(UploadFileDryCallback, len);
        const auto *callback = static_cast<const UploadFileDryCallback *>(data);
        if (callback->result == AckType::SUCCESS) {
            SocketFile2DataHelper::DataFromFileParam param{};
            param.filepath = callback->requestdata.srcfilepath.data();
            param.destfilepath = callback->requestdata.destfilepath.data();
            auto p = helper.DataFromFile<SocketFile2DataHelper::Pass::UPLOAD_FILE>(param);
            if (p) {
                sendContext(p.value(), std::move(callbacks));
            } else {
                callbacks->failure("Failed to prepare file");
            }
        } else {
            callbacks->failure(callback->error_msg.data());
        }
    }
};

namespace {

void initLogging(JNIEnv __unused *env, jobject __unused thiz) {
    absl::InitializeLog();
    LOG(INFO) << __func__;
}

jboolean changeDestinationInfo(JNIEnv *env, jobject __unused thiz, jstring ipaddr,
                               jint type, jint port) {
    auto sockIntf = TgBotSocketNative::getInstance();
    std::string newAddress = Convert<std::string>(env, ipaddr);
    SocketConfig config{};

    switch (type) {
        case AF_INET:
            config.mode = SocketConfig::USE_IPV4;
            break;
        case AF_INET6:
            config.mode = SocketConfig::USE_IPV6;
            break;
        default:
            LOG(ERROR) << "Unknown af type:" << type;
            return JNI_FALSE;
    }
    config.address = newAddress;
    config.port = port;
    LOG(INFO) << "Switched to IP " << std::quoted(newAddress, '\'')
                   << ", af: " << type << ", port: " << port;
    sockIntf->config = config;
    return JNI_TRUE;
}

jobject getCurrentDestinationInfo(JNIEnv *env, __unused jobject thiz) {
    auto cf = TgBotSocketNative::getInstance()->config;
    jclass info = env->FindClass(SOCKNATIVE_DSTINFO_JAVACLS);
    jmethodID ctor = env->GetMethodID(info, "<init>",
                                      "(L" STRING_JAVACLS
                                      ";L" SOCKNATIVE_DSTTYPE_JAVACLS ";I)V");
    jclass destType = env->FindClass(SOCKNATIVE_DSTTYPE_JAVACLS);
    jmethodID valueOf =
        env->GetStaticMethodID(destType, "valueOf",
                               "(L" STRING_JAVACLS ";)"
                               "L" SOCKNATIVE_DSTTYPE_JAVACLS ";");
    jobject destTypeVal = nullptr;
    switch (cf.mode) {
        case SocketConfig::USE_IPV4:
            destTypeVal = env->CallStaticObjectMethod(
                destType, valueOf, Convert<jstring>(env, "IPv4"));
            break;
        case SocketConfig::USE_IPV6:
            destTypeVal = env->CallStaticObjectMethod(
                destType, valueOf, Convert<jstring>(env, "IPv6"));
            break;
    }
    return env->NewObject(info, ctor, Convert<jstring>(env, &cf.address),
                          destTypeVal, cf.port);
}
void sendWriteMessageToChatId(JNIEnv *env, jobject __unused thiz, jlong chat_id,
                              jstring text, jobject callback) {
    WriteMsgToChatId data{};
    data.chat = chat_id;
    copyTo(data.message, Convert<std::string>(env, text).c_str());
    auto pkt = Packet(Command::CMD_WRITE_MSG_TO_CHAT_ID, data);
    TgBotSocketNative::getInstance()->sendContext(pkt, env, callback);
}

void getUptime(JNIEnv *env, jobject __unused thiz, jobject callback) {
    auto pkt = Packet(Command::CMD_GET_UPTIME, true);
    TgBotSocketNative::getInstance()->sendContext(pkt, env, callback);
}

void uploadFile(JNIEnv *env, jobject __unused thiz, jstring path, jstring dest_file_path,
                jobject callback) {
    const auto* native = TgBotSocketNative::getInstance();
    SocketFile2DataHelper::DataFromFileParam params{};
    params.filepath = Convert<std::string>(env, path);
    params.destfilepath = Convert<std::string>(env, dest_file_path);
    params.options = native->uploadOptions;

    LOG(INFO) << "===============" << __func__ << "===============";
    LOG(INFO) << std::boolalpha << "overwrite opt: " << params.options.overwrite;
    LOG(INFO) << std::boolalpha << "hash_ignore opt: " << params.options.hash_ignore;
    auto rc =
        helper.DataFromFile<SocketFile2DataHelper::Pass::UPLOAD_FILE_DRY>(params);
    if (rc) {
        native->sendContext(rc.value(), env, callback);
    } else {
        Callbacks(env, callback).failure("Failed to prepare file");
    }
}

void downloadFile(JNIEnv *env, jobject __unused thiz, jstring remote_file_path,
                  jstring local_file_path, jobject callback) {
    DownloadFile req{};
    copyTo(req.filepath, Convert<std::string>(env, remote_file_path).c_str());
    copyTo(req.destfilename,
           Convert<std::string>(env, local_file_path).c_str());
    auto pkt = Packet(Command::CMD_DOWNLOAD_FILE, req);
    TgBotSocketNative::getInstance()->sendContext(pkt, env, callback);
}

void setUploadFileOptions(JNIEnv * __unused env, jobject __unused thiz, jboolean failIfExist, jboolean failIfChecksumMatch) {
    TgBotSocketNative::getInstance()->uploadOptions = {
        .overwrite = failIfExist == JNI_FALSE,
        .hash_ignore = failIfChecksumMatch == JNI_FALSE,
    };
    LOG(INFO) << "===============" << __func__ << "===============";
    LOG(INFO) << std::boolalpha << "overwrite opt: " << (failIfExist == JNI_TRUE);
    LOG(INFO) << std::boolalpha << "hash_ignore opt: " << (failIfChecksumMatch == JNI_TRUE);
}
}  // namespace

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *__unused reserved) {
    static const JNINativeMethod methods[] = {
        NATIVE_METHOD(initLogging, "()V"),
        NATIVE_METHOD(changeDestinationInfo, "(L" STRING_JAVACLS ";II)Z"),
        NATIVE_METHOD(sendWriteMessageToChatId,
                      "(JL" STRING_JAVACLS ";L" SOCKNATIVE_CMDCB_JAVACLS ";)V"),
        NATIVE_METHOD(getUptime, "(L" SOCKNATIVE_CMDCB_JAVACLS ";)V"),
        NATIVE_METHOD(uploadFile, "(L" STRING_JAVACLS ";L" STRING_JAVACLS
                                  ";L" SOCKNATIVE_CMDCB_JAVACLS ";)V"),
        NATIVE_METHOD(downloadFile, "(L" STRING_JAVACLS ";L" STRING_JAVACLS
                                    ";L" SOCKNATIVE_CMDCB_JAVACLS ";)V"),
        NATIVE_METHOD(getCurrentDestinationInfo,
                      "()L" SOCKNATIVE_DSTINFO_JAVACLS ";"),
        NATIVE_METHOD(setUploadFileOptions, "(ZZ)V")
    };

    return JNI_onLoadDef(vm, SOCKNATIVE_JAVACLS, methods,
                         NATIVE_METHOD_SZ(methods));
}
