#pragma once

#include <shared/FileHelperNew.hpp>
#include <shared/PacketParser.hpp>
#include <optional>

namespace TgBotSocket::Client {

/**
 * @brief Build packets for different commands
 */
class PacketBuilder {
public:
    /**
     * @brief Build packet for any command with parsed arguments
     */
    static std::optional<Packet> buildPacket(
        Command cmd,
        char** argv,
        const Packet::Header::session_token_type& token);

private:
    static std::optional<Packet> buildWriteMsg(
        char** argv, const Packet::Header::session_token_type& token);
    
    static std::optional<Packet> buildCtrlSpamBlock(
        char** argv, const Packet::Header::session_token_type& token);
    
    static std::optional<Packet> buildObserveChat(
        char** argv, const Packet::Header::session_token_type& token);
    
    static std::optional<Packet> buildSendFile(
        char** argv, const Packet::Header::session_token_type& token);
    
    static std::optional<Packet> buildObserveAll(
        char** argv, const Packet::Header::session_token_type& token);
    
    static std::optional<Packet> buildGetUptime(
        const Packet::Header::session_token_type& token);
    
    static std::optional<Packet> buildTransferFile(
        Command cmd, char** argv, const Packet::Header::session_token_type& token);
};

}  // namespace TgBotSocket::Client