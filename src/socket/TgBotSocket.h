#pragma once

#include <absl/log/check.h>

#include <boost/crc.hpp>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <string>
#include <type_traits>

#include "SharedMalloc.hpp"
#include "TgBotCommandExport.hpp"

namespace TgBotSocket {
namespace CommandHelpers {
/**
 * @brief Convert TgBotCommand to string
 *
 * @param cmd Command to convert
 * @return std::string string representation of Command enum
 */
std::string toStr(Command cmd);

/**
 * @brief Get count of Command
 *
 * @param cmd Command to get arg count of
 * @return required arg count of Command
 */
int toCount(Command cmd);

/**
 * @brief Check if given command is a client command
 *
 * @param cmd Command to check
 * @return true if given command is a client command, false otherwise
 */
bool isClientCommand(Command cmd);

/**
 * @brief Check if given command is an internal command
 *
 * @param cmd Command to check
 * @return true if given command is an internal command, false otherwise
 */
bool isInternalCommand(Command cmd);

/**
 * @brief Get help text for Command
 *
 * @return std::string help text for Command
 */
std::string getHelpText(void);
}  // namespace CommandHelpers

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
struct Packet {
    static constexpr auto hdr_sz = sizeof(PacketHeader);
    using header_type = PacketHeader;
    header_type header{};
    SharedMalloc data;

    explicit Packet(header_type::length_type length)
        : data(length) {
        header.magic = header_type::MAGIC_VALUE;
        header.data_size = length;
    }

    // Constructor that takes malloc
    template <typename T>
    explicit Packet(Command cmd, T data)
        : Packet(cmd, &data, sizeof(T)) {
        static_assert(!std::is_pointer_v<T>,
                      "This constructor should not be used with a pointer");
    }

    // Constructor that takes pointer, uses malloc but with size
    template <typename T>
    explicit Packet(Command cmd, T in_data, std::size_t size)
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
        memmove(static_cast<char*>(data.get()) + hdr_sz, data.get(),
                header.data_size);
        memcpy(data.get(), &header, hdr_sz);
        return data;
    }
};
}  // namespace TgBotSocket
