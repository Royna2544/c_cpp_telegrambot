#include "CallbackHandler.hpp"

#include <absl/log/log.h>
#include <bot/FileHelperNew.hpp>
#include <bot/PacketParser.hpp>
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

void CallbackHandler::handleTransferFile(const Packet& pkt) {
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
            result.filepath = root["srcfilepath"].asString();
            result.destfilepath = root["destfilepath"].asString();
            
            data::FileTransferMeta::Options options;
            options.overwrite = root["options"]["overwrite"].asBool();
            options.hash_ignore = root["options"]["hash_ignore"].asBool();
            options.dry_run = root["options"]["dry_run"].asBool();
            
            if (!options.hash_ignore && !root.isMember("hash")) {
                LOG(WARNING) << "hash_ignore is false, but hash is not provided.";
                return;
            }
            
            if (root.isMember("hash")) {
                auto parsed = hexDecode<SHA256_DIGEST_LENGTH>(root["hash"].asString());
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
            
            auto result = (*root)["result"].asBool();
            if (result) {
                LOG(INFO) << "Response from server: Success";
            } else {
                LOG(ERROR) << "Response from server: Failed";
                LOG(ERROR) << "Reason: " << (*root)["error_msg"].asString();
                LOG(ERROR) << "Error type: " << (*root)["error_type"].asString();
            }
            break;
        }
        default:
            LOG(ERROR) << "Unhandled payload type for generic ack";
            break;
    }
}

void CallbackHandler::handle(const Packet& pkt) {
    switch (pkt.header.cmd.operator TgBotSocket::Command()) {
        case Command::CMD_GET_UPTIME_CALLBACK:
            handleUptimeCallback(pkt);
            break;
        case Command::CMD_TRANSFER_FILE:
            handleTransferFile(pkt);
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