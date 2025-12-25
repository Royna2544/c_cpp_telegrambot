#pragma once

#include <client/ClientBackend.hpp>
#include <filesystem>
#include <optional>

namespace TgBotSocket::Client {

/**
 * @brief Handle chunked file transfers from client side
 */
class ChunkedTransferClient {
public:
    explicit ChunkedTransferClient(SocketClientWrapper& backend);

    /**
     * @brief Send a file using chunked transfer protocol
     * @param srcpath Source file path on client
     * @param destpath Destination file path on server
     * @param token Session token for authentication
     * @param verify_hash Whether to verify hash after transfer
     * @return true if transfer completed successfully
     */
    bool sendFileChunked(const std::filesystem::path& srcpath,
                         const std::filesystem::path& destpath,
                         const Packet::Header::session_token_type& token,
                         bool verify_hash = true);

    /**
     * @brief Receive a file using chunked transfer protocol
     * @param begin_packet The BEGIN packet received from server
     * @param token Session token for authentication
     * @return true if transfer completed successfully
     */
    bool receiveFileChunked(const Packet& begin_packet,
                            const Packet::Header::session_token_type& token);

private:
    SocketClientWrapper& backend_;
    constexpr static uint32_t DEFAULT_CHUNK_SIZE = 1 * 1024 * 1024;  // 1 MB

    /**
     * @brief Send a single chunk and wait for acknowledgment
     * @param chunk_index Index of the chunk
     * @param chunk_data Pointer to chunk data
     * @param chunk_size Size of the chunk
     * @param token Session token
     * @return true if chunk was acknowledged successfully
     */
    bool sendChunk(uint32_t chunk_index, const uint8_t* chunk_data,
                   uint32_t chunk_size,
                   const Packet::Header::session_token_type& token);
};

}  // namespace TgBotSocket::Client