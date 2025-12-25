#include "PacketUtils.hpp"

#include <absl/log/log.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <api/ByteHelper.hpp>

#include <ResourceManager.hpp>
#include <filesystem>

#include "CommandMap.hpp"
#include "PacketParser.hpp"

namespace fs = std::filesystem;
using namespace TgBotSocket;
using namespace TgBotSocket::callback;

namespace TgBotSocket {

std::string getMIMEType(const ResourceProvider* resource,
                        const std::string& path) {
    static nlohmann::json doc;
    std::string extension = fs::path(path).extension().string();

    static bool once = [resource] {
        std::string_view buf;
        buf = resource->get("mimeData.json");
        try {
            doc = nlohmann::json::parse(buf);
        } catch (const nlohmann::json::parse_error& e) {
            LOG(ERROR) << "Failed to parse mimedata: " << e.what();
        }
        return true;
    }();
    if (!extension.empty()) {
        if (doc.empty()) {
            LOG(ERROR) << "Failed to load mimedata";
            return {};
        }
        // Look for MIME type in json file.
        for (const auto& elem : doc) {
            if (!elem.contains("types")) {
                continue;
            }
            for (const auto& ex : elem["types"]) {
                if (ex.get<std::string>() == extension) {
                    return elem["name"].get<std::string>();
                }
            }
        }
        LOG(WARNING) << "Unknown file extension: '" << extension << "'";
    }
    return "application/octet-stream";
}

Packet toJSONPacket(const GenericAck& ack,
                    const Packet::Header::session_token_type& session_token) {
    nlohmann::json root;
    root["result"] = ack.result == AckType::SUCCESS;
    if (ack.result != AckType::SUCCESS) {
        root["error_msg"] = safeParse(ack.error_msg);
        switch (ack.result.operator TgBotSocket::callback::AckType()) {
            case AckType::ERROR_CLIENT_ERROR:
                root["error_type"] = "CLIENT_ERROR";
                break;
            case AckType::ERROR_COMMAND_IGNORED:
                root["error_type"] = "COMMAND_IGNORED";
                break;
            case AckType::ERROR_INVALID_ARGUMENT:
                root["error_type"] = "INVALID_ARGUMENT";
                break;
            case AckType::ERROR_RUNTIME_ERROR:
                root["error_type"] = "RUNTIME_ERROR";
                break;
            case AckType::ERROR_TGAPI_EXCEPTION:
                root["error_type"] = "TGAPI_EXCEPTION";
                break;
            default:
                DLOG(WARNING)
                    << "Unknown ack type: " << static_cast<int>(ack.result);
                break;
        }
    }
    return nodeToPacket(Command::CMD_GENERIC_ACK, root, session_token);
}

Packet GenericAckToPacket(
    const GenericAck& gn, const TgBotSocket::PayloadType payloadType,
    const TgBotSocket::Packet::Header::session_token_type& token) {
    LOG(INFO) << "Sending ack: " << std::boolalpha
              << (gn.result == AckType::SUCCESS);
    switch (payloadType) {
        case PayloadType::Binary: {
            return createPacket(Command::CMD_GENERIC_ACK, &gn,
                                sizeof(GenericAck),
                                TgBotSocket::PayloadType::Binary, token);
        } break;
        case PayloadType::Json: {
            return toJSONPacket(gn, token);
        } break;
        default:
            LOG(ERROR) << "Invalid payload type for GenericAck";
            return {};
    }
}

template <typename DataT>
bool CHECK_PACKET_SIZE(Packet& pkt) {
    if (pkt.header.data_size != sizeof(DataT)) {
        LOG(WARNING) << fmt::format(
            "Invalid packet size for cmd: {}. Got {}, diff: {}", pkt.header.cmd,
            pkt.header.data_size,
            -(int64_t)(sizeof(DataT) - pkt.header.data_size));
        return false;
    }
    return true;
}

// Explicit template instantiations
template bool CHECK_PACKET_SIZE<TgBotSocket::data::CtrlSpamBlock>(Packet& pkt);

}  // namespace TgBotSocket