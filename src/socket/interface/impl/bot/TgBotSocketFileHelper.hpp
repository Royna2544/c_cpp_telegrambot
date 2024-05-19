#pragma once

#include <socket/TgBotSocket.h>

#include <optional>
#include <string_view>

/**
 * @brief Writes data to a file.
 *
 * This function writes the data pointed to by 'ptr' and of length 'len' to a file.
 *
 * @param ptr Pointer to the data to be written.
 * @param len Length of the data to be written.
 *
 * @return Returns true if the data is successfully written to the file, false otherwise.
 */
bool fileData_tofile(const void* ptr, size_t len);

/**
 * @brief Reads data from a file and creates a TgBotCommandPacket.
 *
 * This function reads data from a file specified by 'filename' and creates a TgBotCommandPacket.
 * The data is then written to a file specified by 'destfilepath'.
 *
 * @param cmd The TgBotCommand to be included in the TgBotCommandPacket.
 * @param filename The name of the file to read data from.
 * @param destfilepath The name of the file to write the data to.
 *
 * @return Returns an optional TgBotCommandPacket containing the command and the data read from the file.
 *         If the file cannot be opened or read, returns an empty optional.
 */
std::optional<TgBotCommandPacket> fileData_fromFile(TgBotCommand cmd,
                                                    const std::string_view filename,
                                                    const std::string_view destfilepath);