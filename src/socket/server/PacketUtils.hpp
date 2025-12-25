#pragma once

#include <socket/api/Callbacks.hpp>
#include <ResourceManager.hpp>
#include <string>
#include <string_view>

namespace TgBotSocket {

/**
 * @brief Gets the MIME type string for a file based on its extension.
 *
 * @param resource The resource provider for loading MIME data.
 * @param path The file path.
 * @return The MIME type string, or "application/octet-stream" if unknown.
 */
std::string getMIMEType(const ResourceProvider* resource,
                        const std::string& path);

/**
 * @brief Converts a GenericAck to a JSON packet.
 *
 * @param ack The generic acknowledgment.
 * @param session_token The session token.
 * @return The created packet.
 */
Packet toJSONPacket(const callback::GenericAck& ack,
                    const Packet::Header::session_token_type& session_token);

/**
 * @brief Converts a GenericAck to a packet based on payload type.
 *
 * @param gn The generic acknowledgment.
 * @param payloadType The payload type (Binary or JSON).
 * @param token The session token.
 * @return The created packet.
 */
Packet GenericAckToPacket(
    const callback::GenericAck& gn, const PayloadType payloadType,
    const Packet::Header::session_token_type& token);

/**
 * @brief Validates packet size for a given data type.
 *
 * @tparam DataT The expected data type.
 * @param pkt The packet to validate.
 * @return true if the size matches, false otherwise.
 */
template <typename DataT>
bool CHECK_PACKET_SIZE(Packet& pkt);

}  // namespace TgBotSocket