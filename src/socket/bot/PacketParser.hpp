#pragma once

#ifdef TgBotSocketParse_JNI_EXPORTS
#define Socket_API
#else
#include <SocketExports.h>
#endif

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
 * This function attempts to decrypt the given packet using the provided context.
 * If successful, it returns `true`. If decryption fails, it returns `false`.
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

}  // namespace TgBotSocket