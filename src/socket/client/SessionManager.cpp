#include "SessionManager.hpp"

#include <absl/log/log.h>
#include <bot/PacketParser.hpp>
#include <algorithm>

namespace TgBotSocket::Client {

SessionManager::SessionManager(SocketClientWrapper& backend)
    : backend_(backend) {}

std::optional<Packet::Header::session_token_type> SessionManager::openSession() {
    // Send open session request
    Packet openSession = createPacket(
        Command::CMD_OPEN_SESSION, nullptr, 0, PayloadType::Binary, {});
    
    backend_->write(openSession);
    DLOG(INFO) << "Wrote open session packet";

    // Read response
    auto openSessionAck = TgBotSocket::readPacket(backend_.chosen_interface());
    if (!openSessionAck ||
        openSessionAck->header.cmd != Command::CMD_OPEN_SESSION_ACK) {
        LOG(ERROR) << "Failed to open session";
        return std::nullopt;
    }

    // Parse session token from response
    auto _root = parseAndCheck(openSessionAck->data.get(),
                               openSessionAck->data.size(),
                               {"session_token", "expiration_time"});
    if (!_root) {
        LOG(ERROR) << "Invalid open session ack json";
        return std::nullopt;
    }

    auto root = *_root;
    LOG(INFO) << "Opened session. Token: " << root["session_token"]
              << " Expiration_time: " << root["expiration_time"];

    std::string session_token_str = root["session_token"].asString();
    if (session_token_str.size() != Crypto::SESSION_TOKEN_LENGTH) {
        LOG(ERROR) << "Invalid session token length";
        return std::nullopt;
    }

    Packet::Header::session_token_type session_token{};
    std::ranges::copy_n(session_token_str.begin(), Crypto::SESSION_TOKEN_LENGTH,
                        session_token.begin());

    return session_token;
}

bool SessionManager::closeSession(const Packet::Header::session_token_type& token) {
    auto closePacket = createPacket(
        Command::CMD_CLOSE_SESSION, nullptr, 0, PayloadType::Binary, token);
    
    if (!backend_->write(closePacket)) {
        LOG(ERROR) << "Failed to close session";
        return false;
    }
    
    return true;
}

}  // namespace TgBotSocket::Client