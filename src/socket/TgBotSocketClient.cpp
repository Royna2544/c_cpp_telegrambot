#include <absl/log/log.h>

#include <AbslLogInit.hpp>
#include <ManagedThreads.hpp>
#include <TgBotSocket_Export.hpp>
#include <TryParseStr.hpp>
#include <boost/crc.hpp>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <impl/bot/ClientBackend.hpp>
#include <impl/bot/TgBotPacketParser.hpp>
#include <impl/bot/TgBotSocketFileHelper.hpp>
#include <iostream>
#include <optional>
#include <string>

#include "TgBotCommandMap.hpp"
#include "Types.h"

using namespace TgBotSocket;

namespace {

[[noreturn]] void usage(const char* argv, bool success) {
    std::cout << "Usage: " << argv << " [cmd enum value] [args...]" << std::endl
              << std::endl;
    std::cout << "Available cmd enum values:" << std::endl;
    std::cout << CommandHelpers::getHelpText();

    exit(static_cast<int>(!success));
}

bool verifyArgsCount(Command cmd, int argc) {
    int required = CommandHelpers::toCount(cmd);
    if (required != argc) {
        LOG(ERROR) << "Invalid argument count " << argc << " for cmd "
                   << CommandHelpers::toStr(cmd) << ", " << required
                   << " required";
        return false;
    }
    return true;
}

template <size_t N>
void copyToStrBuf(std::array<char, N>& dst, char* src) {
    memset(dst.data(), 0, dst.size());
    strncpy(dst.data(), src, dst.size() - 1);
}

template <class C>
bool parseOneEnum(C* res, C max, const char* str, const char* name) {
    int parsed{};
    if (try_parse(str, &parsed)) {
        if (parsed >= 0 && parsed < static_cast<int>(max)) {
            *res = static_cast<C>(parsed);
            return true;
        } else {
            LOG(ERROR) << "Cannot convert " << str << " to " << name
                       << " enum value";
        }
    }
    return false;
}

std::string_view AckTypeToStr(callback::AckType type) {
    using callback::AckType;
    using callback::GenericAck;
    switch (type) {
        case AckType::SUCCESS:
            return "Success";
        case AckType::ERROR_TGAPI_EXCEPTION:
            return "Failed: Telegram Api exception";
        case AckType::ERROR_INVALID_ARGUMENT:
            return "Failed: Invalid argument";
        case AckType::ERROR_COMMAND_IGNORED:
            return "Failed: Command ignored";
        case AckType::ERROR_RUNTIME_ERROR:
            return "Failed: Runtime error";
    }
    return "Unknown ack type";
}

}  // namespace

struct ClientParser : TgBotSocketParser {
    explicit ClientParser(SocketClientWrapper wrapper)
        : TgBotSocketParser(wrapper.getRawInterface()), wrapper(wrapper) {}
    void handle_CommandPacket(SocketConnContext context, Packet pkt) override {
        using callback::AckType;
        using callback::GenericAck;

        std::string resultText;

        switch (pkt.header.cmd) {
            case Command::CMD_GET_UPTIME_CALLBACK: {
                callback::GetUptimeCallback callbackData = {};
                pkt.data.assignTo(callbackData);
                LOG(INFO) << "Server replied: " << callbackData.uptime.data();
                break;
            }
            case Command::CMD_DOWNLOAD_FILE_CALLBACK: {
                FileDataHelper::DataToFile<FileDataHelper::DOWNLOAD_FILE>(
                    pkt.data.get(), pkt.header.data_size);
                break;
            }
            case Command::CMD_UPLOAD_FILE_DRY_CALLBACK: {
                callback::UploadFileDryCallback callbackData{};
                pkt.data.assignTo(callbackData);
                LOG(INFO) << "Response from server: "
                          << AckTypeToStr(callbackData.result);
                if (callbackData.result != AckType::SUCCESS) {
                    LOG(ERROR) << "Reason: " << callbackData.error_msg.data();
                } else {
                    wrapper->closeSocketHandle(context);
                    auto it = wrapper->createClientSocket();
                    if (!it) {
                        LOG(ERROR) << "Failed to recreate client socket";
                        return;
                    }
                    context = it.value();
                    LOG(INFO) << "Recreated client socket";
                    auto params_in = callbackData.requestdata;
                    FileDataHelper::DataFromFileParam param;
                    param.destfilepath = params_in.destfilepath.data();
                    param.filepath = params_in.srcfilepath.data();
                    param.options = params_in.options;
                    auto newPkt = FileDataHelper::DataFromFile<
                        FileDataHelper::UPLOAD_FILE>(param);
                    LOG(INFO) << "Sending the actual file content again...";
                    wrapper->writeToSocket(context, newPkt->toSocketData());
                    onNewBuffer(context);
                }
                break;
            }
            case Command::CMD_GENERIC_ACK: {
                callback::GenericAck callbackData{};
                pkt.data.assignTo(callbackData);
                LOG(INFO) << "Response from server: "
                          << AckTypeToStr(callbackData.result);
                if (callbackData.result != AckType::SUCCESS) {
                    LOG(ERROR) << "Reason: " << callbackData.error_msg.data();
                }
                break;
            }
            default:
                LOG(ERROR) << "Unhandled callback of command: "
                           << static_cast<int>(pkt.header.cmd);
                break;
        }
        wrapper->closeSocketHandle(context);
    }
    SocketClientWrapper wrapper;
};

int main(int argc, char** argv) {
    enum Command cmd {};
    std::optional<Packet> pkt;
    const char* exe = argv[0];

    TgBot_AbslLogInit();
    if (argc == 1) {
        usage(exe, true);
    }
    // Remove exe (argv[0])
    ++argv;
    --argc;

    if (!parseOneEnum(&cmd, Command::CMD_MAX, *argv, "cmd")) {
        LOG(ERROR) << "Invalid cmd enum value";
        usage(exe, false);
    }

    if (CommandHelpers::isInternalCommand(cmd)) {
        LOG(ERROR) << "Internal commands not supported";
        return EXIT_FAILURE;
    }

    // Remove cmd (argv[1])
    ++argv;
    --argc;

    if (!verifyArgsCount(cmd, argc)) {
        usage(exe, false);
    }

    switch (cmd) {
        case Command::CMD_WRITE_MSG_TO_CHAT_ID: {
            data::WriteMsgToChatId data{};
            ChatId id;
            if (!try_parse(argv[0], &id)) {
                break;
            }
            data.chat = id;
            copyToStrBuf(data.message, argv[1]);
            pkt = Packet(cmd, data);
            break;
        }
        case Command::CMD_CTRL_SPAMBLOCK: {
            data::CtrlSpamBlock data;
            if (parseOneEnum(&data, data::CtrlSpamBlock::CTRL_MAX, argv[0],
                             "spamblock"))

                pkt = Packet(cmd, data);
            break;
        }
        case Command::CMD_OBSERVE_CHAT_ID: {
            data::ObserveChatId data{};
            bool observe;
            ChatId id;
            if (try_parse(argv[0], &id) && try_parse(argv[1], &observe)) {
                data.chat = id;
                data.observe = observe;
                pkt = Packet(cmd, data);
            }
            break;
        }
        case Command::CMD_SEND_FILE_TO_CHAT_ID: {
            data::SendFileToChatId data{};
            ChatId id;
            data::FileType fileType;
            if (try_parse(argv[0], &id) &&
                parseOneEnum(&fileType, data::FileType::TYPE_MAX, argv[1],
                             "type")) {
                data.chat = id;
                data.fileType = fileType;
                copyToStrBuf(data.filePath, argv[2]);
                pkt = Packet(cmd, data);
            }
        } break;
        case Command::CMD_OBSERVE_ALL_CHATS: {
            data::ObserveAllChats data{};
            bool observe;
            if (try_parse(argv[0], &observe)) {
                data.observe = observe;
                pkt = Packet(cmd, data);
            }
        } break;
        case Command::CMD_DELETE_CONTROLLER_BY_ID: {
            ThreadManager::Usage data{};
            if (parseOneEnum(&data, ThreadManager::Usage::MAX, argv[0],
                             "usage")) {
                pkt = Packet(cmd, data);
            }
        }
        case Command::CMD_GET_UPTIME: {
            // Data is unused in this case
            pkt = Packet(cmd, 1);
            break;
        }
        case Command::CMD_UPLOAD_FILE: {
            FileDataHelper::DataFromFileParam params;
            params.filepath = argv[0];
            params.destfilepath = argv[1];
            params.options.hash_ignore = false;  //= true;
            params.options.overwrite = true;
            pkt = FileDataHelper::DataFromFile<FileDataHelper::UPLOAD_FILE_DRY>(
                params);
            break;
        }
        case Command::CMD_DOWNLOAD_FILE: {
            data::DownloadFile data{};
            copyToStrBuf(data.filepath, argv[0]);
            copyToStrBuf(data.destfilename, argv[1]);
            pkt = Packet(cmd, data);
            break;
        }
        default:
            LOG(FATAL) << "Unhandled command: " << CommandHelpers::toStr(cmd);
    };

    if (!pkt) {
        LOG(ERROR) << "Failed parsing arguments for "
                   << CommandHelpers::toStr(cmd).c_str();
        return EXIT_FAILURE;
    } else {
        boost::crc_32_type crc;
        crc.process_bytes(pkt->data.get(), pkt->header.data_size);
        pkt->header.checksum = crc.checksum();
    }

    auto backend = SocketClientWrapper();
    auto handle = backend->createClientSocket();

    if (handle) {
        backend->writeToSocket(handle.value(), pkt->toSocketData());
        LOG(INFO) << "Sent the command: Waiting for callback...";
        // Handle callbacks
        ClientParser parser(backend);
        parser.onNewBuffer(handle.value());
    }

    return static_cast<int>(!pkt.has_value());
}
