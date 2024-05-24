#pragma once

#include <absl/log/check.h>

#include <boost/crc.hpp>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <type_traits>

#include "SharedMalloc.hpp"

inline std::filesystem::path getSocketPath() {
    static auto spath = std::filesystem::temp_directory_path() / "tgbot.sock";
    return spath;
}

#define TGBOT_NAMESPACE_BEGIN namespace TgBotCommandData {
#define TGBOT_NAMESPACE_END }
#include "TgBotCommandExport.hpp"

namespace TgBotCmd {
/**
 * @brief Convert TgBotCommand to string
 *
 * @param cmd TgBotCommand to convert
 * @return std::string string representation of TgBotCommand enum
 */
std::string toStr(TgBotCommand cmd);

/**
 * @brief Get count of TgBotCommand
 *
 * @param cmd TgBotCommand to get arg count of
 * @return required arg count of TgBotCommand
 */
int toCount(TgBotCommand cmd);

/**
 * @brief Check if given command is a client command
 *
 * @param cmd TgBotCommand to check
 * @return true if given command is a client command, false otherwise
 */
bool isClientCommand(TgBotCommand cmd);

/**
 * @brief Check if given command is an internal command
 *
 * @param cmd TgBotCommand to check
 * @return true if given command is an internal command, false otherwise
 */
bool isInternalCommand(TgBotCommand cmd);

/**
 * @brief Get help text for TgBotCommand
 *
 * @return std::string help text for TgBotCommand
 */
std::string getHelpText(void);
}  // namespace TgBotCmd

/**
 * @brief Packet for sending commands to the server
 *
 * @code data_ptr is only vaild for this scope: This should not be sent, instead
 * it must be memcpy'd
 *
 * This packet is used to send commands to the server.
 * It contains a header, which contains the magic value, the command, and the
 * size of the data. The data is then followed by the actual data.
 */
struct TgBotCommandPacket {
    static constexpr auto hdr_sz = sizeof(TgBotCommandPacketHeader);
    using header_type = TgBotCommandPacketHeader;
    header_type header{};
    SharedMalloc data;

    explicit TgBotCommandPacket(header_type::length_type length)
        : data(length) {
        header.magic = header_type::MAGIC_VALUE;
        header.data_size = length;
    }

    // Constructor that takes malloc
    template <typename T>
    explicit TgBotCommandPacket(TgBotCommand cmd, T data)
        : TgBotCommandPacket(cmd, &data, sizeof(T)) {
        static_assert(!std::is_pointer_v<T>,
                      "This constructor should not be used with a pointer");
    }

    // Constructor that takes pointer, uses malloc but with size
    template <typename T>
    explicit TgBotCommandPacket(TgBotCommand cmd, T in_data, std::size_t size)
        : data(size) {
        boost::crc_32_type crc;

        static_assert(std::is_pointer_v<T>,
                      "This constructor should not be used with non pointer");
        header.cmd = cmd;
        header.magic = header_type::MAGIC_VALUE;
        header.data_size = size;
        memcpy(data.get(), in_data, header.data_size);
        crc.process_bytes(data.get(), header.data_size);
        header.checksum = crc.checksum();
    }

    // Converts to full SocketData object, including header
    SharedMalloc toSocketData() {
        data->size = hdr_sz + header.data_size;
        data->alloc();
        memmove(static_cast<char*>(data.get()) + hdr_sz, data.get(), header.data_size);
        memcpy(data.get(), &header, hdr_sz);
        return data;
    }
};