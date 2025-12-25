#include <absl/log/log.h>
#include <fmt/format.h>
#include <fmt/chrono.h>
#include <nlohmann/json.hpp>
#include <tgbot/tools/StringTools.h>

#include <algorithm>
#include <chrono>

#include <shared/PacketParser.hpp>
#include "SocketInterface.hpp"

using namespace TgBotSocket;

bool SocketInterfaceTgBot::verifyHeader(const Packet& packet) {
    decltype(session_table)::iterator iter;

    for (iter = session_table.begin(); iter != session_table.end(); ++iter) {
        if (std::ranges::equal(iter->first, packet.header.session_token)) {
            break;
        }
    }
    if (iter == session_table.end()) {
        LOG(WARNING) << "Received packet with unknown session token";
        return false;
    }
    if (std::chrono::system_clock::now() > iter->second.expiry) {
        LOG(WARNING) << "Session token expired, rejecting and removing";
        session_table.erase(iter);
        return false;
    }
    if (iter->second.last_nonce >= packet.header.nonce) {
        LOG(WARNING) << "Received packet with outdated nonce, ignore";
        return false;
    }
    iter->second.last_nonce = packet.header.nonce;
    return true;
}

void SocketInterfaceTgBot::handle_OpenSession(
    const TgBotSocket::Context& ctx) {
    auto key = StringTools::generateRandomString(
        TgBotSocket::Crypto::SESSION_TOKEN_LENGTH);
    Packet::Header::nounce_type last_nounce{};
    auto tp = std::chrono::system_clock::now() + std::chrono::hours(1);

    LOG(INFO) << "Created new session with key: " << std::quoted(key);
    session_table.emplace(key, Session(key, last_nounce, tp));

    nlohmann::json response;
    response["session_token"] = key;
    response["expiration_time"] = fmt::format("{:%Y-%m-%d %H:%M:%S}", tp);

    Packet::Header::session_token_type session_token{};
    std::ranges::copy_n(key.begin(), Crypto::SESSION_TOKEN_LENGTH,
                        session_token.begin());

    ctx.write(
        nodeToPacket(Command::CMD_OPEN_SESSION_ACK, response, session_token));
}

void SocketInterfaceTgBot::handle_CloseSession(
    const TgBotSocket::Packet::Header::session_token_type& token) {
    decltype(session_table)::iterator iter;
    for (iter = session_table.begin(); iter != session_table.end(); ++iter) {
        if (std::ranges::equal(iter->first, token)) {
            break;
        }
    }
    if (iter == session_table.end()) {
        LOG(WARNING) << "Received packet with unknown session token";
        return;
    }
    session_table.erase(iter);
    LOG(INFO) << "Session with key " << std::string(token.data(), token.size())
              << " closed";
}