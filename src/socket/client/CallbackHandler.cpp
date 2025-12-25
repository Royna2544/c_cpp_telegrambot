#include "CallbackHandler.hpp"
#include "ChunkedTransferClient.hpp"

#include <absl/log/log.h>
#include <nlohmann/json.hpp>
#include <shared/FileHelperNew.hpp>
#include <shared/PacketParser.hpp>
#include <openssl/sha.h>

namespace TgBotSocket::Client {

std::string_view CallbackHandler::ackTypeToString(callback::AckType type) {
    using callback::AckType;
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

std::optional<Packet::Header::length_type> CallbackHandler::findBorderOffset(
    const std::uint8_t* buffer, Packet::Header::length_type size) {
    for (Packet::Header::length_type i = 0; i < size; ++i) {
        if (buffer[i] == data::JSON_BYTE_BORDER) {
            LOG(INFO) << "Found JSON_BYTE_BORDER in offset " << i;
            return i;
        }
    }
    LOG(WARNING) << "JSON_BYTE_BORDER not found in buffer";
    return std::nullopt;
}

void CallbackHandler::handleUptimeCallback(const Packet& pkt) {
    callback::GetUptimeCallback callbackData{};
    pkt.data.assignTo(callbackData);
    LOG(INFO) << "Server replied: " << callbackData.uptime.data();
}

void CallbackHandler::handleTransferFile(const Packet& pkt,
                                         SocketClientWrapper* backend,
                                         const Packet::Header::session_token_type* token) {
    RealFS real;
    SocketFile2DataHelper helper(&real);
    SocketFile2DataHelper::Params result;

    switch (pkt.header.data_type.operator TgBotSocket::PayloadType()) {
        case PayloadType::Binary: {
            if (pkt.data.size() < sizeof(data::FileTransferMeta)) {
                DLOG(WARNING) << "Payload size mismatch on UploadFileMeta";
                return;
            }
            const auto* data = reinterpret_cast<const data::FileTransferMeta*>(
                pkt.data.get());
            result.filepath = safeParse(data->srcfilepath);
            result.destfilepath = safeParse(data->destfilepath);
            result.options = data->options;
            result.hash = data->sha256_hash;
            result.file_size = pkt.data.size() - sizeof(data::FileTransferMeta);
            result.filebuffer = pkt.data.get() + sizeof(data::FileTransferMeta);
            break;
        }
        case PayloadType::Json: {
            const auto offset = findBorderOffset(pkt.data.get(), pkt.data.size())
                                    .value_or(pkt.data.size());
            auto _root = parseAndCheck(pkt.data.get(), pkt.data.size(),
                                       {"srcfilepath", "destfilepath"});
            if (!_root) {
                return;
            }
            auto& root = _root.value();
            result.filepath = root["srcfilepath"].get<std::string>();
            result.destfilepath = root["destfilepath"].get<std::string>();
            
            data::FileTransferMeta::Options options;
            options.overwrite = root["options"]["overwrite"].get<bool>();
            options.hash_ignore = root["options"]["hash_ignore"].get<bool>();
            options.dry_run = root["options"]["dry_run"].get<bool>();
            
            if (!options.hash_ignore && !root.contains("hash")) {
                LOG(WARNING) << "hash_ignore is false, but hash is not provided.";
                return;
            }
            
            if (root.contains("hash")) {
                auto parsed = hexDecode<SHA256_DIGEST_LENGTH>(root["hash"].get<std::string>());
                if (!parsed) {
                    return;
                }
                result.hash = parsed.value();
            }
            
            result.options = options;
            result.file_size = pkt.data.size() - offset;
            result.filebuffer = pkt.data.get() + offset;
            break;
        }
        default:
            LOG(ERROR) << "Invalid payload type for TransferFileMeta";
            return;
    }
    
    helper.ReceiveTransferMeta(result);
}

void CallbackHandler::handleTransferFileBegin(
    const Packet& pkt, SocketClientWrapper* backend,
    const Packet::Header::session_token_type* token) {
    if (!backend || !token) {
        LOG(ERROR) << "Cannot handle chunked transfer without backend/token";
        return;
    }

    LOG(INFO) << "Received CMD_TRANSFER_FILE_BEGIN, starting chunked receive";
    ChunkedTransferClient chunked_client(*backend);
    
    if (!chunked_client.receiveFileChunked(pkt, *token)) {
        LOG(ERROR) << "Chunked file receive failed";
    }
}

void CallbackHandler::handleGenericAck(const Packet& pkt) {
    using callback::AckType;
    
    switch (pkt.header.data_type.operator TgBotSocket::PayloadType()) {
        case PayloadType::Binary: {
            callback::GenericAck callbackData{};
            pkt.data.assignTo(callbackData);
            LOG(INFO) << "Response from server: " << ackTypeToString(callbackData.result);
            if (callbackData.result != AckType::SUCCESS) {
                LOG(ERROR) << "Reason: " << callbackData.error_msg.data();
            }
            break;
        }
        case PayloadType::Json: {
            auto root = parseAndCheck(pkt.data.get(), pkt.data.size(), {"result"});
            if (!root) {
                LOG(ERROR) << "Invalid json in generic ack";
                return;
            }
            
            auto result = (*root)["result"].get<bool>();
            if (result) {
                LOG(INFO) << "Response from server: Success";
            } else {
                LOG(ERROR) << "Response from server: Failed";
                LOG(ERROR) << "Reason: " << (*root)["error_msg"].get<std::string>();
                LOG(ERROR) << "Error type: " << (*root)["error_type"].get<std::string>();
            }
            break;
        }
        default:
            LOG(ERROR) << "Unhandled payload type for generic ack";
            break;
    }
}

void CallbackHandler::handle(const Packet& pkt, SocketClientWrapper* backend,
                              const Packet::Header::session_token_type* token) {
    switch (pkt.header.cmd.operator TgBotSocket::Command()) {
        case Command::CMD_GET_UPTIME_CALLBACK:
            handleUptimeCallback(pkt);
            break;
        case Command::CMD_TRANSFER_FILE:
            handleTransferFile(pkt, backend, token);
            break;
        case Command::CMD_TRANSFER_FILE_BEGIN:
            handleTransferFileBegin(pkt, backend, token);
            break;
        case Command::CMD_GENERIC_ACK:
            handleGenericAck(pkt);
            break;
        default:
            LOG(ERROR) << "Unhandled callback of command: "
                       << static_cast<int>(pkt.header.cmd);
            break;
    }
}

}  // namespace TgBotSocket::Client