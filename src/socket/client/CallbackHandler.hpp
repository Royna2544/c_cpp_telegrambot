#pragma once

#include <ApiDef.hpp>
#include <optional>

namespace TgBotSocket::Client {

/**
 * @brief Handle server callback responses
 */
class CallbackHandler {
public:
    /**
     * @brief Handle a received callback packet
     * @param pkt The packet received from server
     */
    static void handle(const Packet& pkt);

private:
    static void handleUptimeCallback(const Packet& pkt);
    static void handleTransferFile(const Packet& pkt);
    static void handleGenericAck(const Packet& pkt);
    
    static std::string_view ackTypeToString(callback::AckType type);
    static std::optional<Packet::Header::length_type> findBorderOffset(
        const std::uint8_t* buffer, Packet::Header::length_type size);
};

}  // namespace TgBotSocket::Client