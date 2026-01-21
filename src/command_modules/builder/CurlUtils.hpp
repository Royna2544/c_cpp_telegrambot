#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string_view>

namespace CurlUtils {

// Type for cancel checker callback
// A function that returns true if the operation should be cancelled.
// false otherwise.
using CancelChecker = std::function<bool(void)>;

/**
 * Download a file from a URL to a local path.
 * @param url The URL to download from.
 * @param where The local path to save the downloaded file.
 * @param cancel_checker Optional callback to check for cancellation.
 * @return True on success, false on failure.
 */
extern bool download_file(const std::string_view url,
                          const std::filesystem::path& where,
                          CancelChecker cancel_checker = nullptr);

/**
 * Download data from a URL into memory.
 * @param url The URL to download from.
 * @param cancel_checker Optional callback to check for cancellation.
 * @return An optional string containing the downloaded data on success,
 *         or std::nullopt on failure.
 */
extern std::optional<std::string> download_memory(
    const std::string_view url, CancelChecker cancel_checker = nullptr);

/**
 * Send a JSON string via HTTP POST to a URL and get the reply.
 * @param url The URL to send the JSON to.
 * @param json The JSON string to send.
 * @param authkey Optional authorization key to include in the request.
 * @return The reply from the server as a string. Returns std::nullopt on
 * failure.
 */
extern std::optional<std::string> send_json_get_reply(
    const std::string_view url, std::string json,
    const std::string_view authkey = "");

}  // namespace CurlUtils