#pragma once

#include <client/ClientBackend.hpp>
#include <socket/api/Callbacks.hpp>
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
     * @param backend Optional backend for chunked transfers
     * @param token Optional session token for chunked transfers
     */
    static void handle(const Packet& pkt,
                       SocketClientWrapper* backend = nullptr,
                       const Packet::Header::session_token_type* token = nullptr);

private:
    static void handleUptimeCallback(const Packet& pkt);
    static void handleTransferFile(const Packet& pkt,
                                    SocketClientWrapper* backend,
                                    const Packet::Header::session_token_type* token);
    static void handleTransferFileBegin(const Packet& pkt,
                                        SocketClientWrapper* backend,
                                        const Packet::Header::session_token_type* token);
    static void handleGenericAck(const Packet& pkt);
    
    static std::string_view ackTypeToString(callback::AckType type);
    static std::optional<Packet::Header::length_type> findBorderOffset(
        const std::uint8_t* buffer, Packet::Header::length_type size);
};

}  // namespace TgBotSocket::Client