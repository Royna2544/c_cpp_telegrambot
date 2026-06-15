#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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
 * @param authkey Optional authorization key to include in the request.
 * @return An optional string containing the downloaded data on success,
 *         or std::nullopt on failure.
 */
extern std::optional<std::string> download_memory(
    const std::string_view url, CancelChecker cancel_checker = nullptr,
    const std::string_view authkey = "");

/**
 * Download data from a URL into memory, with arbitrary request headers.
 * Use this when the bearer-token convenience overload is not enough (e.g. the
 * Anthropic API requires `x-api-key` and `anthropic-version` headers).
 * @param url The URL to download from.
 * @param cancel_checker Callback to check for cancellation (may be nullptr).
 * @param headers Raw header lines, e.g. "x-api-key: ...".
 * @return An optional string with the downloaded data, or std::nullopt.
 */
extern std::optional<std::string> download_memory(
    const std::string_view url, CancelChecker cancel_checker,
    const std::vector<std::string>& headers);

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

/**
 * Send a JSON string via HTTP POST with arbitrary request headers and get the
 * reply. "Content-Type: application/json" is always added; the given headers
 * are appended (e.g. "x-api-key: ...", "anthropic-version: 2023-06-01").
 * @param url The URL to send the JSON to.
 * @param json The JSON string to send.
 * @param headers Extra raw header lines.
 * @return The reply as a string, or std::nullopt on failure.
 */
extern std::optional<std::string> send_json_get_reply(
    const std::string_view url, std::string json,
    const std::vector<std::string>& headers);

}  // namespace CurlUtils