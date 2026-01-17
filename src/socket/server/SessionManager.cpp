#include <absl/log/log.h>
#include <fmt/format.h>
#include <fmt/chrono.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <random>

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

namespace {

std::string generateRandomString(std::size_t length) {
    static constexpr std::string_view chars =
        "qwertyuiopasdfghjklzxcvbnmQWERTYUIOPASDFGHJKLZXCVBNM1234567890-=[]\\',"
        "./!@#$%^&*()_+{}|:\"<>?`~";
    std::string result;

    result.reserve(length);
    std::generate_n(std::back_inserter(result), length, [&] {
        std::mt19937 gen(std::random_device{}());
        return chars[std::uniform_int_distribution<std::size_t>(
            0, chars.size() - 1)(gen)];
    });
    return result;
}
}

void SocketInterfaceTgBot::handle_OpenSession(
    const TgBotSocket::Context& ctx) {
    auto key = generateRandomString(TgBotSocket::Crypto::SESSION_TOKEN_LENGTH);
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