#include <absl/log/log.h>
#include <openssl/sha.h>

#include <AbslLogInit.hpp>
#include <ManagedThreads.hpp>
#include <TgBotSocket_Export.hpp>
#include <TryParseStr.hpp>
#include <backends/ClientBackend.hpp>
#include <bot/FileHelperNew.hpp>
#include <bot/PacketParser.hpp>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>

#include "SocketContext.hpp"
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
        LOG(ERROR) << fmt::format(
            "Invalid argument count {} for cmd {}, {} required.", argc, cmd,
            required);
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

std::optional<Packet::Header::length_type> findBorderOffset(
    const void* buffer, Packet::Header::length_type size) {
    const auto* iter = static_cast<const uint8_t*>(buffer);
    Packet::Header::length_type offset = 0;
    for (Packet::Header::length_type i = 0; i < size; ++i) {
        if (iter[i] == data::JSON_BYTE_BORDER) {
            LOG(INFO) << "Found JSON_BYTE_BORDER in offset " << i;
            return i;
        }
    }
    LOG(WARNING) << "JSON_BYTE_BORDER not found in buffer";
    return std::nullopt;
}

void handleCallback(SocketClientWrapper& connector, const Packet& pkt) {
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
        case Command::CMD_TRANSFER_FILE: {
            SocketFile2DataHelper::Params result;
            switch (pkt.header.data_type) {
                case PayloadType::Binary: {
                    if (pkt.data.size() < sizeof(data::FileTransferMeta)) {
                        DLOG(WARNING)
                            << "Payload size mismatch on UploadFileMeta";
                        return;
                    }
                    {
                        const auto* data =
                            static_cast<const data::FileTransferMeta*>(
                                pkt.data.get());
                        result.filepath = safeParse(data->srcfilepath);
                        result.destfilepath = safeParse(data->destfilepath);
                        result.options = data->options;
                        result.hash = data->sha256_hash;
                        result.file_size =
                            pkt.data.size() - sizeof(data::FileTransferMeta);
                        result.filebuffer = reinterpret_cast<const uint8_t*>(
                            static_cast<const char*>(pkt.data.get()) +
                            sizeof(data::FileTransferMeta));
                    }
                    break;
                }
                case PayloadType::Json: {
                    const auto offset =
                        findBorderOffset(pkt.data.get(), pkt.data.size())
                            .value_or(pkt.data.size());
                    std::string json(static_cast<const char*>(pkt.data.get()),
                                     offset);
                    auto _root = parseAndCheck(pkt.data.get(), pkt.data.size(),
                                               {"srcfilepath", "destfilepath"});
                    if (!_root) {
                        return;
                    }
                    auto& root = _root.value();
                    result.filepath = root["srcfilepath"].asString();
                    result.destfilepath = root["destfilepath"].asString();
                    data::FileTransferMeta::Options options;
                    options.overwrite = root["options"]["overwrite"].asBool();
                    options.hash_ignore =
                        root["options"]["hash_ignore"].asBool();
                    options.dry_run = root["options"]["dry_run"].asBool();
                    if (!options.hash_ignore && !root.isMember("hash")) {
                        LOG(WARNING) << "hash_ignore is false, but hash is not "
                                        "provided.";
                        return;
                    }
                    if (root.isMember("hash")) {
                        auto parsed = hexDecode<SHA256_DIGEST_LENGTH>(
                            root["hash"].asString());
                        if (!parsed) {
                            return;
                        }
                        result.hash = parsed.value();
                    }
                    result.file_size = pkt.data.size() - offset;
                    result.filebuffer =
                        static_cast<const std::uint8_t*>(pkt.data.get()) +
                        offset;
                    break;
                }
                default:
                    LOG(ERROR) << "Invalid payload type for TransferFileMeta";
                    return;
            }
            helper.ReceiveTransferMeta(result);
            break;
        }
        case Command::CMD_GENERIC_ACK: {
            switch (pkt.header.data_type) {
                case PayloadType::Binary: {
                    callback::GenericAck callbackData{};
                    pkt.data.assignTo(callbackData);
                    LOG(INFO) << "Response from server: "
                              << AckTypeToStr(callbackData.result);
                    if (callbackData.result != AckType::SUCCESS) {
                        LOG(ERROR)
                            << "Reason: " << callbackData.error_msg.data();
                    }
                    break;
                }
                case PayloadType::Json: {
                    auto root = parseAndCheck(pkt.data.get(), pkt.data.size(),
                                              {"result"});
                    if (!root) {
                        LOG(ERROR) << "Invalid json in generic ack";
                        return;
                    }
                    auto result = (*root)["result"].asBool();
                    if (result) {
                        LOG(INFO) << "Response from server: Success";
                    } else {
                        LOG(ERROR) << "Response from server: Failed";
                        LOG(ERROR)
                            << "Reason: " << (*root)["error_msg"].asString();
                        LOG(ERROR) << "Error type: "
                                   << (*root)["error_type"].asString();
                    }
                    break;
                }
                default:
                    LOG(ERROR) << "Unhandled payload type for generic ack";
                    break;
            }
            break;
        }
        default:
            LOG(ERROR) << "Unhandled callback of command: "
                       << static_cast<int>(pkt.header.cmd);
            break;
    }
}

template <typename T>
std::optional<T> parseArgs(char** argv) = delete;

template <>
std::optional<data::WriteMsgToChatId> parseArgs(char** argv) {
    data::WriteMsgToChatId data{};
    if (!try_parse(argv[0], &data.chat)) {
        return std::nullopt;
    }
    copyTo(data.message, argv[1]);
    return data;
}

template <>
std::optional<data::CtrlSpamBlock> parseArgs(char** argv) {
    data::CtrlSpamBlock data;
    if (parseOneEnum(&data, data::CtrlSpamBlock::MAX, argv[0], "spamblock")) {
        return data;
    }
    return std::nullopt;
}

template <>
std::optional<data::ObserveChatId> parseArgs(char** argv) {
    data::ObserveChatId data{};
    if (try_parse(argv[0], &data.chat) && try_parse(argv[1], &data.observe)) {
        return data;
    }
    return std::nullopt;
}

template <>
std::optional<data::SendFileToChatId> parseArgs(char** argv) {
    data::SendFileToChatId data{};
    ChatId id;
    data::FileType fileType;
    if (try_parse(argv[0], &id) &&
        parseOneEnum(&fileType, data::FileType::TYPE_MAX, argv[1], "type")) {
        data.chat = id;
        data.fileType = fileType;
        copyTo(data.filePath, argv[2]);
        return data;
    }
    return std::nullopt;
}

template <>
std::optional<data::ObserveAllChats> parseArgs(char** argv) {
    data::ObserveAllChats data{};
    bool observe = false;
    if (try_parse(argv[0], &observe)) {
        data.observe = observe;
        return data;
    }
    return std::nullopt;
}

struct None {};

template <>
std::optional<None> parseArgs(char** argv) {
    return None{};
}

template <>
std::optional<SocketFile2DataHelper::Params> parseArgs(char** argv) {
    SocketFile2DataHelper::Params params;
    params.filepath = argv[0];
    params.destfilepath = argv[1];
    params.options.hash_ignore = false;  //= true;
    params.options.overwrite = true;
    params.options.dry_run = false;
    return params;
}

int main(int argc, char** argv) {
    enum Command cmd {};
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

    SocketClientWrapper backend;
    if (backend.connect(Context::kTgBotHostPort, Context::hostPath())) {
        DLOG(INFO) << "Connected to server";
        Packet openSession = createPacket(Command::CMD_OPEN_SESSION, nullptr, 0,
                                          PayloadType::Binary, {});
        backend->write(openSession);
        DLOG(INFO) << "Wrote open session packet";
        auto openSessionAck =
            TgBotSocket::readPacket(backend.chosen_interface());
        if (!openSessionAck ||
            openSessionAck->header.cmd != Command::CMD_OPEN_SESSION_ACK) {
            LOG(ERROR) << "Failed to open session";
            return EXIT_FAILURE;
        }
        auto _root = parseAndCheck(openSessionAck->data.get(),
                                   openSessionAck->data.size(),
                                   {"session_token", "expiration_time"});
        if (!_root) {
            LOG(ERROR) << "Invalid open session ack json";
            return EXIT_FAILURE;
        }
        auto root = *_root;
        LOG(INFO) << "Opened session. Token: " << root["session_token"]
                  << " Expiration_time: " << root["expiration_time"];

        std::string session_token_str = root["session_token"].asString();
        if (session_token_str.size() != Packet::Header::SESSION_TOKEN_LENGTH) {
            LOG(ERROR) << "Invalid session token length";
            return EXIT_FAILURE;
        }
        Packet::Header::session_token_type session_token{};
        std::ranges::copy_n(session_token_str.begin(),
                            Packet::Header::SESSION_TOKEN_LENGTH,
                            session_token.begin());

        std::optional<Packet> pkt;
        switch (cmd) {
            case Command::CMD_WRITE_MSG_TO_CHAT_ID: {
                auto args = parseArgs<data::WriteMsgToChatId>(argv);
                if (!args) {
                    usage(exe, false);
                }
                pkt = createPacket(cmd, &args.value(), sizeof(*args),
                                   PayloadType::Binary, session_token);
                break;
            }
            case Command::CMD_CTRL_SPAMBLOCK: {
                auto args = parseArgs<data::CtrlSpamBlock>(argv);
                if (!args) {
                    usage(exe, false);
                }
                pkt = createPacket(cmd, &args.value(), sizeof(*args),
                                   PayloadType::Binary, session_token);
                break;
            }
            case Command::CMD_OBSERVE_CHAT_ID: {
                auto args = parseArgs<data::ObserveChatId>(argv);
                if (!args) {
                    usage(exe, false);
                }
                pkt = createPacket(cmd, &args.value(), sizeof(*args),
                                   PayloadType::Binary, session_token);
                break;
            }
            case Command::CMD_SEND_FILE_TO_CHAT_ID: {
                auto args = parseArgs<data::SendFileToChatId>(argv);
                if (!args) {
                    usage(exe, false);
                }
                pkt = createPacket(cmd, &args.value(), sizeof(*args),
                                   PayloadType::Binary, session_token);
            } break;
            case Command::CMD_OBSERVE_ALL_CHATS: {
                auto args = parseArgs<data::ObserveAllChats>(argv);
                if (!args) {
                    usage(exe, false);
                }
                pkt = createPacket(cmd, &args.value(), sizeof(*args),
                                   PayloadType::Binary, session_token);
            } break;
            case Command::CMD_GET_UPTIME: {
                auto args = parseArgs<None>(argv);
                if (!args) {
                    usage(exe, false);
                }
                pkt = createPacket(cmd, &args.value(), sizeof(*args),
                                   PayloadType::Binary, session_token);
                break;
            }
            case Command::CMD_TRANSFER_FILE:
            case Command::CMD_TRANSFER_FILE_REQUEST: {
                RealFS realfs;
                SocketFile2DataHelper helper(&realfs);
                auto args = parseArgs<SocketFile2DataHelper::Params>(argv);
                if (!args) {
                    usage(exe, false);
                    return EXIT_FAILURE;
                }
                pkt = helper.CreateTransferMeta(
                    args.value(), session_token, PayloadType::Json,
                    cmd == Command::CMD_TRANSFER_FILE_REQUEST);
                break;
            }
            default:
                LOG(FATAL) << fmt::format("Unhandled command: {}", cmd);
        };

        backend->write(*pkt);
        LOG(INFO) << "Sent the command: Waiting for callback...";
        auto it = TgBotSocket::readPacket(backend.chosen_interface());
        if (it) {
            handleCallback(backend, it.value());
        }
        auto closePacket =
            createPacket(TgBotSocket::Command::CMD_CLOSE_SESSION, nullptr, 0,
                         PayloadType::Binary, session_token);
        if (!backend->write(closePacket)) {
            LOG(ERROR) << "Failed to close session";
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}
