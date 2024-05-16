#include <absl/log/initialize.h>
#include <absl/log/log.h>
#include <absl/log/log_sink_registry.h>

#include <LogSinks.hpp>
#include <SocketData.hpp>
#include <TryParseStr.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <impl/bot/ClientBackend.hpp>
#include <impl/bot/TgBotPacketParser.hpp>
#include <iostream>
#include <optional>

#include "SocketDescriptor_defs.hpp"
#include "TgBotSocket.h"
#include "interface/SocketBase.hpp"
#include "interface/impl/bot/TgBotPacketParser.hpp"

[[noreturn]] static void usage(const char* argv, bool success) {
    std::cout << "Usage: " << argv << " [cmd enum value] [args...]" << std::endl
              << std::endl;
    std::cout << "Available cmd enum values:" << std::endl;
    std::cout << TgBotCmd::getHelpText();

    exit(!success);
}

static bool verifyArgsCount(TgBotCommand cmd, int argc) {
    int required = TgBotCmd::toCount(cmd);
    if (required != argc) {
        fprintf(stderr, "Invalid argument count %d for cmd %s, %d required\n",
                argc, TgBotCmd::toStr(cmd).c_str(), required);
        return false;
    }
    return true;
}

static void _copyToStrBuf(char dst[], size_t dst_size, char* src) {
    memset(dst, 0, dst_size);
    strncpy(dst, src, dst_size - 1);
}

template <unsigned N>
void copyToStrBuf(char (&dst)[N], char* src) {
    _copyToStrBuf(dst, sizeof(dst), src);
}

template <class C>
bool parseOneEnum(C* res, C max, const char* str, const char* name) {
    int parsed = 0;
    if (try_parse(str, &parsed)) {
        if (max >= 0 && parsed < max) {
            *res = static_cast<C>(parsed);
            return true;
        } else {
            fprintf(stderr, "Cannot convert %s to %s enum value\n", str, name);
        }
    }
    return false;
}

struct ClientParser : TgBotSocketParser {
    explicit ClientParser(SocketInterfaceBase* interface)
        : TgBotSocketParser(interface) {}
    void handle_CommandPacket(SocketConnContext context,
                              TgBotCommandPacket pkt) override {
        switch (pkt.header.cmd) {
            case CMD_GET_UPTIME_CALLBACK: {
                TgBotCommandData::GetUptimeCallback callbackData = {};
                memcpy(callbackData, pkt.data_ptr.getData(),
                       sizeof(callbackData));
                printf("Server replied: %s\n", callbackData);
                interface->closeSocketHandle(context);
                break;
            }
            default:
                LOG(ERROR) << "Unhandled callback of command: "
                           << pkt.header.cmd;
                break;
        }
    }
};

int main(int argc, char** argv) {
    enum TgBotCommand cmd = CMD_MAX;
    std::optional<TgBotCommandPacket> pkt;
    const char* exe = argv[0];

    if (argc == 1) {
        usage(exe, true);
    }
    absl::InitializeLog();
    StdFileSink sink;
    absl::AddLogSink(&sink);

    // Remove exe (argv[0])
    ++argv;
    --argc;

    if (parseOneEnum(&cmd, CMD_MAX, *argv, "cmd")) {
        if (TgBotCmd::isInternalCommand(cmd)) {
            fprintf(stderr, "Internal commands not supported\n");
        } else {
            // Remove cmd (argv[1])
            ++argv;
            --argc;

            if (verifyArgsCount(cmd, argc)) {
                switch (cmd) {
                    case CMD_WRITE_MSG_TO_CHAT_ID: {
                        TgBotCommandData::WriteMsgToChatId data{};
                        if (!try_parse(argv[0], &data.to)) {
                            break;
                        }
                        copyToStrBuf(data.msg, argv[1]);
                        pkt = TgBotCommandPacket(cmd, data);
                        break;
                    }
                    case CMD_CTRL_SPAMBLOCK: {
                        TgBotCommandData::CtrlSpamBlock data;
                        if (parseOneEnum(&data, TgBotCommandData::CTRL_MAX,
                                         argv[0], "spamblock"))

                            pkt = TgBotCommandPacket(cmd, data);
                        break;
                    }
                    case CMD_OBSERVE_CHAT_ID: {
                        TgBotCommandData::ObserveChatId data{};
                        if (try_parse(argv[0], &data.id) &&
                            try_parse(argv[1], &data.observe))
                            pkt = TgBotCommandPacket(cmd, data);
                    } break;
                    case CMD_SEND_FILE_TO_CHAT_ID: {
                        TgBotCommandData::SendFileToChatId data{};
                        if (try_parse(argv[0], &data.id) &&
                            parseOneEnum(&data.type, TYPE_MAX, argv[1],
                                         "type")) {
                            copyToStrBuf(data.filepath, argv[2]);
                            pkt = TgBotCommandPacket(cmd, data);
                        }
                    } break;
                    case CMD_OBSERVE_ALL_CHATS: {
                        TgBotCommandData::ObserveAllChats data = false;
                        if (try_parse(argv[0], &data)) {
                            pkt = TgBotCommandPacket(cmd, data);
                        }
                    } break;
                    case CMD_DELETE_CONTROLLER_BY_ID: {
                        TgBotCommandData::DeleteControllerById data{};
                        if (try_parse(argv[0], &data)) {
                            pkt = TgBotCommandPacket(cmd, data);
                        }
                    }
                    case CMD_GET_UPTIME: {
                        // Data is unused in this case
                        pkt = TgBotCommandPacket(cmd, 1);
                        break;
                    }
                    default:
                        LOG(FATAL)
                            << "Unhandled command: " << TgBotCmd::toStr(cmd);
                };
                if (!pkt)
                    fprintf(stderr, "Failed parsing arguments for %s\n",
                            TgBotCmd::toStr(cmd).c_str());
            }
        }
    }
    auto* backend = getClientBackend();
    auto handle = backend->createClientSocket();
    if (!pkt) {
        usage(exe, false);
    }

    if (handle) {
        backend->writeToSocket(handle.value(), pkt->toSocketData());
        // Handle callbacks
        switch (cmd) {
            case CMD_GET_UPTIME: {
                ClientParser parser(backend);
                DLOG(INFO) << "Waiting for callback...";
                parser.onNewBuffer(handle.value());
                break;
            }
            default:
                break;
        }
    }

    return static_cast<int>(!pkt.has_value());
}
