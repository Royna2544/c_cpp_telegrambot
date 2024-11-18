#include <absl/log/log.h>

#include <AbslLogInit.hpp>
#include <ManagedThreads.hpp>
#include <TgBotSocket_Export.hpp>
#include <TryParseStr.hpp>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <impl/backends/ClientBackend.hpp>
#include <impl/bot/TgBotPacketParser.hpp>
#include <impl/bot/TgBotSocketFileHelperNew.hpp>
#include <iostream>
#include <optional>
#include <string>

#include "SocketBase.hpp"
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
        LOG(ERROR) << fmt::format("Invalid argument count {} for cmd {}, {} required.", argc, cmd, required);
        return false;
    }
    return true;
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
        case AckType::ERROR_CLIENT_ERROR:
            return "Failed: Client error";
    }
    return "Unknown ack type";
}

}  // namespace

void handle_CommandPacket(const SocketClientWrapper& wrapper,
                          SocketConnContext& context, const Packet& pkt) {
    using callback::AckType;
    using callback::GenericAck;
    std::string resultText;

    RealFS real;
    SocketFile2DataHelper helper(&real);

    switch (pkt.header.cmd) {
        case Command::CMD_GET_UPTIME_CALLBACK: {
            callback::GetUptimeCallback callbackData = {};
            pkt.data.assignTo(callbackData);
            LOG(INFO) << "Server replied: " << callbackData.uptime.data();
            break;
        }
        case Command::CMD_DOWNLOAD_FILE_CALLBACK: {
            helper.DataToFile<SocketFile2DataHelper::Pass::DOWNLOAD_FILE>(
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
                SocketFile2DataHelper::DataFromFileParam param;
                param.destfilepath = params_in.destfilepath.data();
                param.filepath = params_in.srcfilepath.data();
                param.options = params_in.options;
                auto newPkt =
                    helper
                        .DataFromFile<SocketFile2DataHelper::Pass::UPLOAD_FILE>(
                            param);
                LOG(INFO) << "Sending the actual file content again...";
                wrapper->writeToSocket(context, newPkt->toSocketData());
                auto v = wrapper.getRawInterface();
                auto it2 =
                    TgBotSocket::readPacket(v.get(), context);
                if (it2) {
                    handle_CommandPacket(wrapper, it.value(), it2.value());
                }
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

    RealFS realfs;
    SocketFile2DataHelper helper(&realfs);

    switch (cmd) {
        case Command::CMD_WRITE_MSG_TO_CHAT_ID: {
            data::WriteMsgToChatId data{};
            ChatId id;
            if (!try_parse(argv[0], &id)) {
                break;
            }
            data.chat = id;
            copyTo(data.message, argv[1]);
            pkt = Packet(cmd, data);
            break;
        }
        case Command::CMD_CTRL_SPAMBLOCK: {
            data::CtrlSpamBlock data;
            if (parseOneEnum(&data, data::CtrlSpamBlock::MAX, argv[0],
                             "spamblock")) {
                pkt = Packet(cmd, data);
            }
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
                copyTo(data.filePath, argv[2]);
                pkt = Packet(cmd, data);
            }
        } break;
        case Command::CMD_OBSERVE_ALL_CHATS: {
            data::ObserveAllChats data{};
            bool observe = false;
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
            SocketFile2DataHelper::DataFromFileParam params;
            params.filepath = argv[0];
            params.destfilepath = argv[1];
            params.options.hash_ignore = false;  //= true;
            params.options.overwrite = true;
            pkt =
                helper
                    .DataFromFile<SocketFile2DataHelper::Pass::UPLOAD_FILE_DRY>(
                        params);
            break;
        }
        case Command::CMD_DOWNLOAD_FILE: {
            data::DownloadFile data{};
            copyTo(data.filepath, argv[0]);
            copyTo(data.destfilename, argv[1]);
            pkt = Packet(cmd, data);
            break;
        }
        default:
            LOG(FATAL) << fmt::format("Unhandled command: {}", cmd);
    };

    if (!pkt) {
        LOG(ERROR) << fmt::format("Failed parsing arguments for {}", cmd);
        return EXIT_FAILURE;
    } else {
        pkt->header.checksum = pkt->crc32_function(pkt->data);
    }

    SocketClientWrapper backend(
        SocketInterfaceBase::LocalHelper::getSocketPath());
    backend->options.use_connect_timeout.set(true);
    backend->options.connect_timeout.set(3s);
    auto handle = backend->createClientSocket();

    if (handle) {
        backend->writeToSocket(handle.value(), pkt->toSocketData());
        LOG(INFO) << "Sent the command: Waiting for callback...";
        auto b = backend.getRawInterface();
        auto it =
            TgBotSocket::readPacket(b.get(), handle.value());
        if (it) {
            handle_CommandPacket(backend, handle.value(), it.value());
        }
    }

    return static_cast<int>(!pkt.has_value());
}
