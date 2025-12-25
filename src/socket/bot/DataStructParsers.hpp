#pragma once

#include <ApiDef.hpp>
#include <cstdint>
#include <filesystem>
#include <optional>

#include "FileHelperNew.hpp"

namespace TgBotSocket {

struct WriteMsgToChatId {
    ChatId chat;
    std::string message;

    static std::optional<WriteMsgToChatId> fromBuffer(
        const std::uint8_t* buffer, Packet::Header::length_type size,
        PayloadType type);
};

struct ObserveChatId {
    ChatId chat;
    bool observe;

    static std::optional<ObserveChatId> fromBuffer(
        const std::uint8_t* buffer, Packet::Header::length_type size,
        PayloadType type);
};

struct SendFileToChatId {
    ChatId chat;
    TgBotSocket::data::FileType fileType;
    std::filesystem::path filePath;

    static std::optional<SendFileToChatId> fromBuffer(
        const std::uint8_t* buffer, Packet::Header::length_type size,
        PayloadType type);
};

struct ObserveAllChats {
    bool observe;

    static std::optional<ObserveAllChats> fromBuffer(
        const std::uint8_t* buffer, Packet::Header::length_type size,
        PayloadType type);
};

struct TransferFileMeta : SocketFile2DataHelper::Params {
    static std::optional<TransferFileMeta> fromBuffer(
        const uint8_t* buffer, Packet::Header::length_type size,
        PayloadType type);
};

/**
 * @brief Finds the JSON byte border offset in a buffer.
 *
 * @param buffer The buffer to search.
 * @param size The size of the buffer.
 * @return The offset if found, std::nullopt otherwise.
 */
std::optional<Packet::Header::length_type> findBorderOffset(
    const uint8_t* buffer, Packet::Header::length_type size);

}  // namespace TgBotSocket