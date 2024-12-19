#pragma once

#include <SocketExports.h>
#include <absl/strings/escaping.h>
#include <json/json.h>

#include <SocketContext.hpp>
#include <optional>

#include "TgBotSocket_Export.hpp"

namespace TgBotSocket {

/**
 * @brief Reads a packet from the socket using the provided context.
 *
 * This function attempts to read a packet from the socket associated with the
 * given context. If successful, it returns an optional containing the read
 * packet. If no packet is available, it returns an empty optional.
 *
 * @param context The context to use for reading the packet.
 * @return An optional containing the read packet, or an empty optional if no
 * packet is available.
 */
std::optional<Packet> Socket_API
readPacket(const TgBotSocket::Context& context);

/**
 * @brief Decrypts a packet using the provided context.
 *
 * This function attempts to decrypt the given packet using the provided
 * context. If successful, it returns `true`. If decryption fails, it returns
 * `false`.
 *
 * @param packet The packet to decrypt
 * @return `true` if the packet was successfully decrypted; otherwise, `false`.
 */
bool Socket_API decryptPacket(TgBotSocket::Packet& packet);

/**
 * @brief Creates a packet with the given command and data.
 *
 * This function creates a packet with the specified command and data. The
 * length of the data is determined by the provided length parameter.
 *
 * @param command The command to set in the packet header.
 * @param data A pointer to the data to be included in the packet.
 * @param length The length of the data in bytes.
 * @param payloadType The type of payload used in the packet.
 * @param sessionToken The session token to set in the packet header.
 * @return The created packet.
 */
Packet Socket_API
createPacket(const Command command, const void* data,
             Packet::Header::length_type length, const PayloadType payloadType,
             const Packet::Header::session_token_type& sessionToken);

/**
 * @brief Parses a buffer and checks for the presence of specified JSON nodes.
 *
 * This function takes a buffer containing JSON data, parses it, and checks if
 * the specified nodes are present in the parsed JSON object.
 *
 * @param buf Pointer to the buffer containing the JSON data.
 * @param length Length of the buffer.
 * @param nodes List of JSON node names to check for in the parsed JSON object.
 * @return std::optional<Json::Value> The parsed JSON object if parsing is
 *         successful and all specified nodes are present, std::nullopt
 * otherwise.
 */
std::optional<Json::Value> Socket_API
parseAndCheck(const void* buf, TgBotSocket::Packet::Header::length_type length,
              const std::initializer_list<const char*> nodes);

/**
 * @brief Converts a command and JSON value into a Packet.
 *
 * @param command The command to be converted.
 * @param json The JSON value containing additional data for the packet.
 * @param session_token The session token to be included in the packet header.
 * @return Packet The resulting packet created from the command and JSON value.
 */
Packet Socket_API
nodeToPacket(const Command& command, const Json::Value& json,
             const Packet::Header::session_token_type& session_token);

template <size_t N>
std::string safeParse(const std::array<char, N>& buf) {
    // Create a larger array to hold the null-terminated string
    std::array<char, N + 1> safebuf{};

    // Safely copy N characters from buf to safebuf
    std::ranges::copy_n(buf.begin(), N, safebuf.begin());

    // Null-terminate the new buffer
    safebuf[N] = '\0';

    return safebuf.data();
}

template <size_t N>
std::optional<std::array<std::uint8_t, N>> hexDecode(
    const absl::string_view hexEncoded) {
    std::string binary;

    if (hexEncoded.empty()) {
        LOG(ERROR) << "Invalid hex string, empty";
        return std::nullopt;
    }
    
    if (!absl::HexStringToBytes(hexEncoded, &binary)) {
        LOG(ERROR) << "Invalid hex string, HexStringToBytes failed";
        return std::nullopt;
    }

    if (binary.empty() || binary.size() != N) {  // Validate size
        LOG(ERROR) << "Invalid hex string length or content";
        return std::nullopt;
    }

    std::array<std::uint8_t, N> result{};
    std::copy(binary.begin(), binary.end(), result.begin());
    return result;
}

template <size_t N>
std::string hexEncode(const std::array<std::uint8_t, N>& data) {
    return absl::BytesToHexString(absl::string_view(reinterpret_cast<const char*>(data.data()), data.size()));
}

}  // namespace TgBotSocket