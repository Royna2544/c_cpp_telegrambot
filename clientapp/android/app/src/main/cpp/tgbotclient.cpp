#include <absl/log/initialize.h>
#include <android/log.h>
#include <arpa/inet.h>
#include <cerrno>
#include <jni.h>
#include <unistd.h>
#include <zlib.h>
#include <sys/poll.h>

#include <cstring>
#include <string>
#include <string_view>

#include "JNIOnLoad.h"
#include "JavaCppConverter.hpp"
// Include the tgbot's exported header
#include "../../../../../../src/socket/include/TgBotSocket_Export.hpp"
#include "../../../../../../src/socket/interface/impl/bot/TgBotSocketFileHelper.hpp"

using namespace TgBotSocket;
using namespace TgBotSocket::data;
using namespace TgBotSocket::callback;

struct SocketConfig {
    std::string address;
    enum { USE_IPV4, USE_IPV6 } mode;
    int port;

    int timeout_s = 5;
};

#define SOCKNATIVE_JAVACLS "com/royna/tgbotclient/SocketCommandNative"
#define SOCKNATIVE_CMDCB_JAVACLS SOCKNATIVE_JAVACLS "$ICommandCallback"
#define SOCKNATIVE_DSTINFO_JAVACLS SOCKNATIVE_JAVACLS "$DestinationInfo"
#define SOCKNATIVE_DSTTYPE_JAVACLS SOCKNATIVE_JAVACLS "$DestinationType"
#define STRING_JAVACLS "java/lang/String"

class TgBotSocketNative {
   public:
    void sendContext(Packet &packet, JNIEnv *env, jobject callbackObj) const {
        switch (config.mode) {
            case SocketConfig::USE_IPV4:
                sendContextCommon<AF_INET, sockaddr_in>(packet, env,
                                                        callbackObj);
                break;
            case SocketConfig::USE_IPV6:
                sendContextCommon<AF_INET6, sockaddr_in6>(packet, env,
                                                          callbackObj);
                break;
            default:
                ABSL_LOG(ERROR)
                    << "Unknown mode: " << static_cast<int>(config.mode);
                break;
        }
    }

    void setSocketConfig(SocketConfig config_in) {
        config = std::move(config_in);
    }
    SocketConfig getSocketConfig() { return config; }

    void setUploadFileOptions(UploadFile::Options options_in) {
        uploadOptions = options_in;
    }
    UploadFile::Options getUploadFileOptions() { return uploadOptions; }

    static std::shared_ptr<TgBotSocketNative> getInstance() {
        static auto instance =
            std::make_shared<TgBotSocketNative>(TgBotSocketNative());
        return instance;
    }

    static void callSuccess(JNIEnv *env, jobject callbackObj,
                            jobject resultObj) {
        jclass callbackClass = env->FindClass(SOCKNATIVE_CMDCB_JAVACLS);
        jmethodID success = env->GetMethodID(callbackClass, "onSuccess",
                                             "(Ljava/lang/Object;)V");
        ABSL_DLOG(INFO) << "Will execute: " << __func__;
        env->CallVoidMethod(callbackObj, success, resultObj);
        ABSL_LOG(INFO) << "Called onSuccess callback";
    }

    static void callFailed(JNIEnv *env, jobject callbackObj,
                           std::string message) {
        jclass callbackClass = env->FindClass(SOCKNATIVE_CMDCB_JAVACLS);
        jmethodID failed =
            env->GetMethodID(callbackClass, "onError", "(L" STRING_JAVACLS ";)V");

        ABSL_DLOG(INFO) << "Will execute: " << __func__;
        env->CallVoidMethod(callbackObj, failed, Convert<jstring>(env, message));
        ABSL_LOG(INFO) << "Called onFailed callback";
    }

   private:
    constexpr static int kRecvFlags = MSG_WAITALL | MSG_NOSIGNAL;
    constexpr static int kSendFlags = MSG_NOSIGNAL;
    SocketConfig config;
    UploadFile::Options uploadOptions;

    TgBotSocketNative() = default;

    static void closeFd(int *fd) { close(*fd); }

    static void _callFailed(JNIEnv *env, jobject callbackObj,
                            std::string message) {
        ABSL_PLOG(ERROR) << message;
        callFailed(env, callbackObj, message);
    }

    template <int af, typename SockAddr>
    void sendContextCommon(Packet &context, JNIEnv *env,
                           jobject callbackObj) const {
        SockAddr addr{};
        socklen_t len = sizeof(SockAddr);
        int sockfd{};
        int ret{};

        ABSL_LOG(INFO) << "Prepare to send CommandContext";
        sockfd = socket(af, SOCK_STREAM | SOCK_NONBLOCK, 0);
        if (sockfd < 0) {
            _callFailed(env, callbackObj, "Failed to create socket");
            return;
        }

        auto sockFdCloser = std::unique_ptr<int, decltype(&closeFd)>(
            &sockfd, &TgBotSocketNative::closeFd);
        ABSL_LOG(INFO) << "Using IP: " << std::quoted(config.address, '\'')
                       << ", Port: " << config.port;
        setupSockAddress(&addr);

        struct timeval timeout {.tv_sec = 5 };
        setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        // Calculate CRC32
        uLong crc = crc32(0L, Z_NULL, 0);  // Initial value
        crc = crc32(crc, reinterpret_cast<Bytef *>(context.data.get()),
                    context.header.data_size);
        context.header.checksum = crc;

        if (connect(sockfd, reinterpret_cast<sockaddr *>(&addr), len) != 0) {
            if (errno != EINPROGRESS) {
                _callFailed(env, callbackObj, "Failed to initiate connect()");
                return;
            }
        }

        // Wait for the nonblocking socket's event...
        struct pollfd fds{};
        fds.events = POLLOUT;
        fds.fd = sockfd;
        ret = poll(&fds, 1, config.timeout_s * 1000);
        if (ret < 0) {
            _callFailed(env, callbackObj, "Failed to poll()");
            return;
        } else if (!(fds.revents & POLLOUT)) {
            _callFailed(env, callbackObj, "The server didn't respond");
            return;
        }

        // Get if it failed...
        int error = 0;
        socklen_t error_len = sizeof(error);
        ret = getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &error, &error_len);
        if (ret < 0) {
            _callFailed(env, callbackObj, "Failed to getsockopt()");
            return;
        }

        // If failed to connect, abort
        if (error != 0) {
            callFailed(env, callbackObj, "Failed to connect to server");
            return;
        }

        ret = fcntl(sockfd, F_GETFL);
        if (ret < 0) {
            _callFailed(env, callbackObj, "Failed to get socket fd flags");
            return;
        }
        if (!(ret & O_NONBLOCK)) {
            ret = fcntl(sockfd, F_SETFL, ret & ~O_NONBLOCK);
            if (ret < 0) {
                _callFailed(env, callbackObj, "Failed to set socket fd blocking");
                return;
            }
        }

        ABSL_LOG(INFO) << "Connected to server";
        ret = send(sockfd, &context.header, sizeof(PacketHeader), kSendFlags);
        if (ret < 0) {
            _callFailed(env, callbackObj, "Failed to send packet header");
            return;
        } else {
            ABSL_LOG(INFO) << "Sent header packet with cmd "
                           << static_cast<int>(context.header.cmd) << ", "
                           << ret << " bytes";
        }
        ret = send(sockfd, context.data.get(), context.header.data_size,
                   kSendFlags);
        if (ret < 0) {
            _callFailed(env, callbackObj, "Failed to send packet data");
            return;
        } else {
            ABSL_LOG(INFO) << "Sent data packet, " << ret << " bytes";
        }
        ABSL_LOG(INFO) << "Done sending data";
        ABSL_LOG(INFO) << "Now reading callback";

#define RETRY(exp) ({         \
    __typeof__(exp) _rc;                   \
    do {                                   \
        _rc = (exp);                       \
    } while (_rc == -1 && (errno == EINTR || errno == EAGAIN)); \
    _rc; })

        PacketHeader header;
        ret = RETRY(recv(sockfd, &header, sizeof(header), kRecvFlags));
        if (ret < 0) {
            _callFailed(env, callbackObj, "Failed to read callback header");
            return;
        }
        if (header.magic != PacketHeader ::MAGIC_VALUE) {
            _callFailed(env, callbackObj, "Bad magic value of callback header");
            return;
        }
        ABSL_DLOG(INFO) << "Allocating " << header.data_size << " bytes...";
        SharedMalloc data(header.data_size);
        if (data.get() == nullptr) {
            if (header.data_size == 0) {
                callFailed(env, callbackObj, "Server returned 0 bytes of data");
            } else {
                errno = ENOMEM;
                _callFailed(env, callbackObj,
                            "Failed to alloc data for callback header");
            }
            return;
        }
        ret = RETRY(recv(sockfd, data.get(), header.data_size, kRecvFlags));
        if (ret < 0) {
            _callFailed(env, callbackObj, "Failed to read callback data");
            return;
        }
        ABSL_LOG(INFO) << "Command received: " << static_cast<int>(header.cmd);
        switch (header.cmd) {
            case Command::CMD_GET_UPTIME_CALLBACK:
                handleSpecificData<Command::CMD_GET_UPTIME_CALLBACK>(
                    env, callbackObj, data.get(), header.data_size);
                break;
            case Command::CMD_DOWNLOAD_FILE_CALLBACK:
                handleSpecificData<Command::CMD_DOWNLOAD_FILE_CALLBACK>(
                    env, callbackObj, data.get(), header.data_size);
                break;
            case Command::CMD_UPLOAD_FILE_DRY_CALLBACK:
                handleSpecificData<Command::CMD_UPLOAD_FILE_DRY_CALLBACK>(
                        env, callbackObj, data.get(), header.data_size);
                break;
            default: {
                GenericAck AckData{};
                bool success{};

                memcpy(&AckData, data.get(), sizeof(AckData));
                success = AckData.result == AckType::SUCCESS;
                ABSL_LOG(INFO) << "Command ACK: " << std::boolalpha << success;
                if (!success) {
                    ABSL_LOG(ERROR) << "Reason: " << AckData.error_msg.data();
                    callFailed(env, callbackObj, AckData.error_msg.data());
                } else {
                    callSuccess(env, callbackObj, nullptr);
                }
                break;
            }
        }
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

    template <Command cmd>
    void handleSpecificData(JNIEnv *env, jobject callbackObj, const void *data,
                            PacketHeader::length_type len) const = delete;
    template <>
    void handleSpecificData<Command::CMD_DOWNLOAD_FILE_CALLBACK>(
        JNIEnv *env, jobject callbackObj, const void *data,
        PacketHeader::length_type len) const {
        bool rc =
            FileDataHelper::DataToFile<FileDataHelper::Pass::DOWNLOAD_FILE>(
                data, len);
        if (rc) {
            callSuccess(env, callbackObj, nullptr);
        } else {
            callFailed(env, callbackObj, "Failed to handle download file");
        }
    }
    template <>
    void handleSpecificData<Command::CMD_GET_UPTIME_CALLBACK>(
        JNIEnv *env, jobject callbackObj, const void *data,
        PacketHeader::length_type len) const {
        const auto *uptime = static_cast<const char *>(data);
        callSuccess(env, callbackObj, Convert<jstring>(env, uptime));
    }
    template <>
    void handleSpecificData<Command::CMD_UPLOAD_FILE_DRY_CALLBACK>(
        JNIEnv *env, jobject callbackObj, const void *data,
        PacketHeader::length_type len) const {
        const auto *callback = static_cast<const UploadFileDryCallback *>(data);
        if (callback->result == AckType::SUCCESS) {
            FileDataHelper::DataFromFileParam param{};
            param.filepath = callback->requestdata.srcfilepath.data();
            param.destfilepath = callback->requestdata.destfilepath.data();
            auto p = FileDataHelper::DataFromFile<FileDataHelper::Pass::UPLOAD_FILE>(param);
            if (p) {
                sendContext(p.value(), env, callbackObj);
            } else {
                ABSL_LOG(ERROR) << "Failed to prepare file";
                callFailed(env, callbackObj, "Failed to prepare file");
            }
        } else {
            callFailed(env, callbackObj, callback->error_msg.data());
        }
    }
};

namespace {

void initLogging(JNIEnv *env, jobject thiz) {
    absl::InitializeLog();
    ABSL_LOG(INFO) << __func__;
}

jboolean changeDestinationInfo(JNIEnv *env, jobject thiz, jstring ipaddr,
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
            ABSL_LOG(ERROR) << "Unknown af type:" << type;
            return false;
    }
    config.address = newAddress;
    config.port = port;
    ABSL_LOG(INFO) << "Switched to IP " << std::quoted(newAddress, '\'')
                   << ", af: " << type << ", port: " << port;
    sockIntf->setSocketConfig(config);
    return true;
}
jobject getCurrentDestinationInfo(JNIEnv *env, jobject thiz) {
    auto cf = TgBotSocketNative::getInstance()->getSocketConfig();
    jclass info = env->FindClass(SOCKNATIVE_DSTINFO_JAVACLS);
    jmethodID ctor = env->GetMethodID(info, "<init>",
                                      "(L" STRING_JAVACLS
                                      ";L" SOCKNATIVE_DSTTYPE_JAVACLS ";I)V");
    jclass destType = env->FindClass(SOCKNATIVE_DSTTYPE_JAVACLS);
    jmethodID valueOf =
        env->GetStaticMethodID(destType, "valueOf",
                               "(L" STRING_JAVACLS ";)"
                               "L" SOCKNATIVE_DSTTYPE_JAVACLS ";");
    jobject destTypeVal;
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
    return env->NewObject(info, ctor, Convert<jstring>(env, cf.address),
                          destTypeVal, cf.port);
}
void sendWriteMessageToChatId(JNIEnv *env, jobject thiz, jlong chat_id,
                              jstring text, jobject callback) {
    WriteMsgToChatId data;
    data.chat = chat_id;
    copyTo(data.message, Convert<std::string>(env, text).c_str());
    auto pkt = Packet(Command::CMD_WRITE_MSG_TO_CHAT_ID, data);
    TgBotSocketNative::getInstance()->sendContext(pkt, env, callback);
}

void getUptime(JNIEnv *env, jobject thiz, jobject callback) {
    auto pkt = Packet(Command::CMD_GET_UPTIME, true);
    TgBotSocketNative::getInstance()->sendContext(pkt, env, callback);
}

void uploadFile(JNIEnv *env, jobject thiz, jstring path, jstring dest_file_path,
                jobject callback) {
    FileDataHelper::DataFromFileParam params{};
    params.filepath = Convert<std::string>(env, path);
    params.destfilepath = Convert<std::string>(env, dest_file_path);
    params.options = TgBotSocketNative::getInstance()->getUploadFileOptions();

    ABSL_LOG(INFO) << "===============" << __func__ << "===============";
    ABSL_LOG(INFO) << std::boolalpha << "overwrite opt: " << params.options.overwrite;
    ABSL_LOG(INFO) << std::boolalpha << "hash_ignore opt: " << params.options.hash_ignore;
    auto rc =
        FileDataHelper::DataFromFile<FileDataHelper::Pass::UPLOAD_FILE_DRY>(params);
    if (rc) {
        TgBotSocketNative::getInstance()->sendContext(rc.value(), env,
                                                      callback);
    } else {
        ABSL_LOG(ERROR) << "Failed to prepare file";
        TgBotSocketNative::callFailed(env, callback, "Failed to prepare file");
    }
}

void downloadFile(JNIEnv *env, jobject thiz, jstring remote_file_path,
                  jstring local_file_path, jobject callback) {
    DownloadFile req{};
    copyTo(req.filepath, Convert<std::string>(env, remote_file_path).c_str());
    copyTo(req.destfilename,
           Convert<std::string>(env, local_file_path).c_str());
    auto pkt = Packet(Command::CMD_DOWNLOAD_FILE, req);
    TgBotSocketNative::getInstance()->sendContext(pkt, env, callback);
}

void setUploadFileOptions(JNIEnv *env, jobject thiz, jboolean failIfExist, jboolean failIfChecksumMatch) {
    TgBotSocketNative::getInstance()->setUploadFileOptions({
        .overwrite = !failIfExist,
        .hash_ignore = !failIfChecksumMatch,
    });
    ABSL_LOG(INFO) << "===============" << __func__ << "===============";
    ABSL_LOG(INFO) << std::boolalpha << "overwrite opt: " << !failIfExist;
    ABSL_LOG(INFO) << std::boolalpha << "hash_ignore opt: " << !failIfChecksumMatch;
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
