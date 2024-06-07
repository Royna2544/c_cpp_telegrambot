#include <absl/log/initialize.h>
#include <android/log.h>
#include <arpa/inet.h>
#include <errno.h>
#include <jni.h>
#include <unistd.h>
#include <zlib.h>

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

#define __LOG(level, str) \
    __android_log_print(ANDROID_LOG_##level, "TGBotCli:CPP", "%s", str)

struct SocketConfig {
    std::string address;
    enum { USE_IPV4, USE_IPV6 } mode;
};

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

    static std::shared_ptr<TgBotSocketNative> getInstance() {
        static auto instance =
            std::make_shared<TgBotSocketNative>(TgBotSocketNative());
        return instance;
    }

    static void callSuccess(JNIEnv *env, jobject callbackObj,
                            jobject resultObj) {
        jclass callbackClass = env->FindClass(kCallbackCls.data());
        jmethodID success = env->GetMethodID(callbackClass, "onSuccess",
                                             "(Ljava/lang/Object;)V");
        env->CallVoidMethod(callbackObj, success, resultObj);
        ABSL_LOG(INFO) << "Called onSuccess callback";
    }

    static void callFailed(JNIEnv *env, jobject callbackObj,
                           std::string message) {
        jclass callbackClass = env->FindClass(kCallbackCls.data());
        jmethodID success =
                env->GetMethodID(callbackClass, "onError", "(Ljava/lang/String;)V");
        env->CallVoidMethod(callbackObj, success,
                            Convert<jstring>(env, message));
        ABSL_LOG(INFO) << "Called onFailed callback";
    }

   private:
    constexpr static int kSocketPort = 50000;
    constexpr static int kRecvFlags = MSG_WAITALL | MSG_NOSIGNAL;
    static constexpr std::string_view kCallbackCls =
            "com/royna/tgbotclient/SocketCommandNative$ICommandCallback";
    SocketConfig config;

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
        sockfd = socket(af, SOCK_STREAM, 0);
        if (sockfd < 0) {
            _callFailed(env, callbackObj, "Failed to create socket");
            return;
        }

        auto sockFdCloser = std::unique_ptr<int, decltype(&closeFd)>(
            &sockfd, &TgBotSocketNative::closeFd);
        ABSL_LOG(INFO) << "Using IP: " << std::quoted(config.address, '\'')
                       << ", Port: " << kSocketPort;
        setupSockAddress(&addr);

        // Calculate CRC32
        uLong crc = crc32(0L, Z_NULL, 0);  // Initial value
        crc = crc32(crc, reinterpret_cast<Bytef *>(context.data.get()),
                    context.header.data_size);
        context.header.checksum = crc;

        if (connect(sockfd, reinterpret_cast<sockaddr *>(&addr), len) != 0) {
            _callFailed(env, callbackObj, "Failed to connect to server");
            return;
        }
        ABSL_LOG(INFO) << "Connected to server";
        ret = send(sockfd, &context.header, sizeof(PacketHeader), MSG_NOSIGNAL);
        if (ret < 0) {
            _callFailed(env, callbackObj, "Failed to send packet header");
            return;
        } else {
            ABSL_LOG(INFO) << "Sent header packet with cmd "
                           << static_cast<int>(context.header.cmd) << ", "
                           << ret << " bytes";
        }
        ret = send(sockfd, context.data.get(), context.header.data_size,
                   MSG_NOSIGNAL);
        if (ret < 0) {
            _callFailed(env, callbackObj, "Failed to send packet data");
            return;
        } else {
            ABSL_LOG(INFO) << "Sent data packet, " << ret << " bytes";
        }
        ABSL_LOG(INFO) << "Done sending data";
        ABSL_LOG(INFO) << "Now reading callback";

        PacketHeader header;
        ret = recv(sockfd, &header, sizeof(header), kRecvFlags);
        if (ret < 0) {
            _callFailed(env, callbackObj, "Failed to read callback header");
            return;
        }
        if (header.magic != PacketHeader ::MAGIC_VALUE) {
            _callFailed(env, callbackObj, "Bad magic value of callback header");
            return;
        }
        SharedMalloc data(malloc(header.data_size));
        if (data.get() == nullptr) {
            errno = ENOMEM;
            _callFailed(env, callbackObj,
                        "Failed to alloc data for callback header");
            return;
        }
        ret = recv(sockfd, data.get(), header.data_size, kRecvFlags);
        if (ret < 0) {
            _callFailed(env, callbackObj, "Failed to read callback data");
            return;
        }
        switch (header.cmd) {
            case Command::CMD_GET_UPTIME_CALLBACK:
                handleSpecificData<Command::CMD_GET_UPTIME_CALLBACK>(
                    env, callbackObj, data.get(), header.data_size);
                break;
            case Command::CMD_DOWNLOAD_FILE_CALLBACK:
                handleSpecificData<Command::CMD_DOWNLOAD_FILE_CALLBACK>(
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
        addr->sin_port = htons(kSocketPort);
    }
    template <>
    [[maybe_unused]] void setupSockAddress(sockaddr_in6 *addr) const {
        addr->sin6_family = AF_INET6;
        inet_pton(AF_INET6, config.address.c_str(), &addr->sin6_addr);
        addr->sin6_port = htons(kSocketPort);
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
};

namespace {

void initLogging(JNIEnv *env, jobject thiz) {
    absl::InitializeLog();
    ABSL_LOG(INFO) << __func__;
}

jboolean changeDestinationInfo(JNIEnv *env, jobject thiz, jstring ipaddr,
                               jint type) {
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
    ABSL_LOG(INFO) << "Switched to IP " << std::quoted(newAddress, '\'')
                   << ", af: " << type;
    sockIntf->setSocketConfig(config);
    return true;
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
    FileDataHelper::DataFromFileParam params;
    params.filepath = Convert<std::string>(env, path);
    params.destfilepath = Convert<std::string>(env, dest_file_path);
    auto rc =
        FileDataHelper::DataFromFile<FileDataHelper::Pass::UPLOAD_FILE>(params);
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
    copyTo(req.destfilename, Convert<std::string>(env, local_file_path).c_str());
    auto pkt = Packet(Command::CMD_DOWNLOAD_FILE, req);
    TgBotSocketNative::getInstance()->sendContext(pkt, env, callback);
}
}  // namespace

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *__unused reserved) {
    static const JNINativeMethod methods[] = {
        NATIVE_METHOD(initLogging, "()V"),
        NATIVE_METHOD(changeDestinationInfo, "(Ljava/lang/String;I)Z"),
        NATIVE_METHOD(sendWriteMessageToChatId,
                      "(JLjava/lang/String;Lcom/royna/tgbotclient/"
                      "SocketCommandNative$ICommandCallback;)V"),
        NATIVE_METHOD(
            getUptime,
            "(Lcom/royna/tgbotclient/SocketCommandNative$ICommandCallback;)V"),
        NATIVE_METHOD(uploadFile,
                      "(Ljava/lang/String;Ljava/lang/String;Lcom/royna/"
                      "tgbotclient/SocketCommandNative$ICommandCallback;)V"),
        NATIVE_METHOD(downloadFile,
                      "(Ljava/lang/String;Ljava/lang/String;Lcom/royna/"
                      "tgbotclient/SocketCommandNative$ICommandCallback;)V")};
    return JNI_onLoadDef(vm, "com/royna/tgbotclient/SocketCommandNative",
                         methods, NATIVE_METHOD_SZ(methods));
}
