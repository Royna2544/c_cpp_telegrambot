#pragma once

#include <TgBotSocket_Export.hpp>

#include <SocketBase.hpp>
#include <optional>

#include "SharedMalloc.hpp"

struct TgBotSocketParser {
    enum class HandleState {
        Ok,      // OK: Continue parsing
        Ignore,  // Ignore: Parse was completed, skip to next buf
        Fail     // Fail: Parse failed, exit loop
    };

    bool onNewBuffer(SocketConnContext ctx);

    /**
     * @brief Reads a packet header from the socket.
     *
     * @param socketData [in] The SharedMalloc object of incoming data.
     * @param pkt [out] The TgBotCommandPacket to fill.
     *
     * @return HandleState object containing the state.
     */
    [[nodiscard]] static HandleState handle_PacketHeader(
        std::optional<SharedMalloc> &socketData,
        std::optional<TgBotSocket::Packet> &pkt);

    /**
     * @brief Reads a packet from the socket.
     *
     * @param socketData The SocketData object of data.
     * @param pkt The TgBotCommandPacket to read the packet into.
     *
     * @return HandleState object containing the state.
     */
    [[nodiscard]] static HandleState handle_Packet(
        std::optional<SharedMalloc> &socketData,
        std::optional<TgBotSocket::Packet> &pkt);

    virtual void handle_CommandPacket(SocketConnContext ctx,
                                      TgBotSocket::Packet commandPacket) = 0;

    explicit TgBotSocketParser(SocketInterfaceBase *interface)
        : interface(interface) {}

   private:
    SocketInterfaceBase *interface;
};
