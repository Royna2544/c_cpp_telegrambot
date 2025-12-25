#include "ChunkedTransferClient.hpp"

#include <absl/log/log.h>
#include <shared/FileHelperNew.hpp>
#include <shared/PacketParser.hpp>
#include <api/Callbacks.hpp>

namespace TgBotSocket::Client {

ChunkedTransferClient::ChunkedTransferClient(SocketClientWrapper& backend)
    : backend_(backend) {}

bool ChunkedTransferClient::sendFileChunked(
    const std::filesystem::path& srcpath,
    const std::filesystem::path& destpath,
    const Packet::Header::session_token_type& token, bool verify_hash) {
    // Read the file
    RealFS realfs;
    auto file_data = realfs.readFile(srcpath);
    if (!file_data) {
        LOG(ERROR) << "Failed to read file: " << srcpath;
        return false;
    }

    // Calculate hash
    HashContainer hash;
    realfs.SHA256(file_data.value(), hash);

    const uint64_t total_size = file_data->size();
    const uint32_t chunk_size = DEFAULT_CHUNK_SIZE;
    const uint32_t num_chunks = (total_size + chunk_size - 1) / chunk_size;

    LOG(INFO) << "Starting chunked file transfer: " << srcpath << " -> "
              << destpath << " (" << total_size << " bytes, " << num_chunks
              << " chunks)";

    // Send BEGIN packet
    TgBotSocket::data::FileTransferBegin begin_data{
        .destfilepath = {},
        .total_size = total_size,
        .chunk_size = chunk_size,
        .sha256_hash = hash.m_data,
    };
    copyTo(begin_data.destfilepath, destpath.string());

    auto begin_packet =
        createPacket(Command::CMD_TRANSFER_FILE_BEGIN, &begin_data,
                     sizeof(begin_data), PayloadType::Binary, token);

    if (!backend_->write(begin_packet)) {
        LOG(ERROR) << "Failed to send BEGIN packet";
        return false;
    }

    // Wait for BEGIN acknowledgment
    auto ack = readPacket(backend_.chosen_interface());
    if (!ack || ack->header.cmd != Command::CMD_GENERIC_ACK) {
        LOG(ERROR) << "Failed to receive BEGIN acknowledgment";
        return false;
    }

    // Check if BEGIN was successful
    callback::GenericAck begin_ack{};
    ack->data.assignTo(begin_ack);
    if (begin_ack.result != callback::AckType::SUCCESS) {
        LOG(ERROR) << "Server rejected BEGIN: "
                   << begin_ack.error_msg.data();
        return false;
    }

    LOG(INFO) << "BEGIN acknowledged, starting chunk transfer";

    // Send all chunks
    const uint8_t* data_ptr = static_cast<const uint8_t*>(file_data->get());
    for (uint32_t chunk_index = 0; chunk_index < num_chunks; ++chunk_index) {
        const uint32_t offset = chunk_index * chunk_size;
        const uint32_t current_chunk_size =
            std::min(chunk_size, static_cast<uint32_t>(total_size - offset));

        if (!sendChunk(chunk_index, data_ptr + offset, current_chunk_size,
                       token)) {
            LOG(ERROR) << "Failed to send chunk " << chunk_index;
            return false;
        }

        // Log progress every 10 chunks or on last chunk
        if (chunk_index % 10 == 0 || chunk_index == num_chunks - 1) {
            const float progress =
                static_cast<float>(chunk_index + 1) / num_chunks * 100.0f;
            LOG(INFO) << "Progress: " << chunk_index + 1 << "/" << num_chunks
                      << " chunks (" << std::fixed << std::setprecision(1)
                      << progress << "%)";
        }
    }

    LOG(INFO) << "All chunks sent, sending END packet";

    // Send END packet
    TgBotSocket::data::FileTransferEnd end_data{.verify_hash = verify_hash};

    auto end_packet =
        createPacket(Command::CMD_TRANSFER_FILE_END, &end_data,
                     sizeof(end_data), PayloadType::Binary, token);

    if (!backend_->write(end_packet)) {
        LOG(ERROR) << "Failed to send END packet";
        return false;
    }

    // Wait for END acknowledgment
    auto end_ack = readPacket(backend_.chosen_interface());
    if (!end_ack || end_ack->header.cmd != Command::CMD_GENERIC_ACK) {
        LOG(ERROR) << "Failed to receive END acknowledgment";
        return false;
    }

    // Check if END was successful
    callback::GenericAck end_result{};
    end_ack->data.assignTo(end_result);
    if (end_result.result != callback::AckType::SUCCESS) {
        LOG(ERROR) << "Server rejected END: " << end_result.error_msg.data();
        return false;
    }

    LOG(INFO) << "File transfer completed successfully";
    return true;
}

bool ChunkedTransferClient::sendChunk(
    uint32_t chunk_index, const uint8_t* chunk_data, uint32_t chunk_size,
    const Packet::Header::session_token_type& token) {
    // Allocate buffer for chunk packet (header + data)
    const size_t packet_size = sizeof(data::FileTransferChunk) + chunk_size;
    SharedMalloc chunk_buffer(packet_size);

    // Fill in chunk header
    data::FileTransferChunk chunk_header{
        .chunk_index = chunk_index,
        .chunk_data_size = chunk_size,
    };

    chunk_buffer.assignFrom(chunk_header);
    chunk_buffer.assignFrom(chunk_data, chunk_size,
                            sizeof(data::FileTransferChunk));

    // Create and send packet
    auto chunk_packet = createPacket(
        Command::CMD_TRANSFER_FILE_CHUNK, chunk_buffer.get(),
        chunk_buffer.size(), PayloadType::Binary, token);

    if (!backend_->write(chunk_packet)) {
        LOG(ERROR) << "Failed to send chunk " << chunk_index;
        return false;
    }

    // Wait for chunk acknowledgment
    auto response = readPacket(backend_.chosen_interface());
    if (!response ||
        response->header.cmd != Command::CMD_TRANSFER_FILE_CHUNK_RESPONSE) {
        LOG(ERROR) << "Failed to receive chunk response for chunk "
                   << chunk_index;
        return false;
    }

    // Parse response
    if (response->data.size() < sizeof(data::FileTransferChunkResponse)) {
        LOG(ERROR) << "Invalid chunk response size";
        return false;
    }

    data::FileTransferChunkResponse chunk_response{};
    response->data.assignTo(chunk_response);

    if (!chunk_response.success) {
        LOG(ERROR) << "Chunk " << chunk_index << " rejected: "
                   << chunk_response.error_msg.data();
        return false;
    }

    if (chunk_response.chunk_index != chunk_index) {
        LOG(ERROR) << "Chunk index mismatch: expected " << chunk_index
                   << ", got " << chunk_response.chunk_index;
        return false;
    }

    DLOG(INFO) << "Chunk " << chunk_index << " acknowledged";
    return true;
}

bool ChunkedTransferClient::receiveFileChunked(
    const Packet& begin_packet,
    const Packet::Header::session_token_type& token) {
    // Parse BEGIN packet
    if (begin_packet.data.size() < sizeof(data::FileTransferBegin)) {
        LOG(ERROR) << "Invalid BEGIN packet size";
        return false;
    }

    data::FileTransferBegin begin_data{};
    begin_packet.data.assignTo(begin_data);

    const std::filesystem::path destpath = safeParse(begin_data.destfilepath);
    const uint64_t total_size = begin_data.total_size;
    const uint32_t chunk_size = begin_data.chunk_size;
    const auto expected_hash = begin_data.sha256_hash;
    const uint32_t num_chunks = (total_size + chunk_size - 1) / chunk_size;

    LOG(INFO) << "Receiving chunked file: " << destpath << " (" << total_size
              << " bytes, " << num_chunks << " chunks)";

    // Send BEGIN acknowledgment
    callback::GenericAck begin_ack = callback::GenericAck::ok();
    auto ack_packet = createPacket(Command::CMD_GENERIC_ACK, &begin_ack,
                                    sizeof(begin_ack), PayloadType::Binary,
                                    token);

    if (!backend_->write(ack_packet)) {
        LOG(ERROR) << "Failed to send BEGIN acknowledgment";
        return false;
    }

    // Receive all chunks
    std::vector<uint8_t> file_buffer;
    file_buffer.reserve(total_size);

    for (uint32_t chunk_index = 0; chunk_index < num_chunks; ++chunk_index) {
        auto chunk_packet = readPacket(backend_.chosen_interface());
        if (!chunk_packet ||
            chunk_packet->header.cmd != Command::CMD_TRANSFER_FILE_CHUNK) {
            LOG(ERROR) << "Failed to receive chunk " << chunk_index;
            return false;
        }

        // Parse chunk
        if (chunk_packet->data.size() <
            sizeof(data::FileTransferChunk)) {
            LOG(ERROR) << "Invalid chunk packet size";
            return false;
        }

        data::FileTransferChunk chunk_data{};
        chunk_packet->data.assignTo(chunk_data);

        if (chunk_data.chunk_index != chunk_index) {
            LOG(ERROR) << "Chunk index mismatch: expected " << chunk_index
                       << ", got " << chunk_data.chunk_index;
            return false;
        }

        // Extract chunk data
        const uint32_t expected_size =
            sizeof(data::FileTransferChunk) + chunk_data.chunk_data_size;
        if (chunk_packet->data.size() < expected_size) {
            LOG(ERROR) << "Chunk data size mismatch";
            return false;
        }

        const uint8_t* chunk_bytes =
            chunk_packet->data.get() + sizeof(data::FileTransferChunk);
        file_buffer.insert(file_buffer.end(), chunk_bytes,
                           chunk_bytes + chunk_data.chunk_data_size);

        // Send chunk acknowledgment
        data::FileTransferChunkResponse response{
            .chunk_index = chunk_index,
            .success = true,
        };
        copyTo(response.error_msg, "OK");

        auto response_packet =
            createPacket(Command::CMD_TRANSFER_FILE_CHUNK_RESPONSE, &response,
                         sizeof(response), PayloadType::Binary, token);

        if (!backend_->write(response_packet)) {
            LOG(ERROR) << "Failed to send chunk acknowledgment";
            return false;
        }

        // Log progress
        if (chunk_index % 10 == 0 || chunk_index == num_chunks - 1) {
            const float progress =
                static_cast<float>(chunk_index + 1) / num_chunks * 100.0f;
            LOG(INFO) << "Progress: " << chunk_index + 1 << "/" << num_chunks
                      << " chunks (" << std::fixed << std::setprecision(1)
                      << progress << "%)";
        }
    }

    // Verify we received all data
    if (file_buffer.size() != total_size) {
        LOG(ERROR) << "Size mismatch: received " << file_buffer.size()
                   << " bytes, expected " << total_size;
        return false;
    }

    // Wait for END packet
    auto end_packet = readPacket(backend_.chosen_interface());
    if (!end_packet ||
        end_packet->header.cmd != Command::CMD_TRANSFER_FILE_END) {
        LOG(ERROR) << "Failed to receive END packet";
        return false;
    }

    data::FileTransferEnd end_data{};
    end_packet->data.assignTo(end_data);

    // Verify hash if requested
    if (end_data.verify_hash) {
        LOG(INFO) << "Verifying file hash...";
        RealFS realfs;
        HashContainer computed_hash{};
        SharedMalloc file_mem(file_buffer.size());
        file_mem.assignFrom(file_buffer.data(), file_buffer.size());
        realfs.SHA256(file_mem, computed_hash);

        if (!std::ranges::equal(computed_hash.m_data, expected_hash)) {
            LOG(ERROR) << "Hash verification failed";
            
            // Send error acknowledgment
            callback::GenericAck end_ack(callback::AckType::ERROR_RUNTIME_ERROR,
                                         "Hash verification failed");
            auto end_ack_packet =
                createPacket(Command::CMD_GENERIC_ACK, &end_ack,
                             sizeof(end_ack), PayloadType::Binary, token);
            backend_->write(end_ack_packet);
            return false;
        }
        LOG(INFO) << "Hash verification successful";
    }

    // Write file to disk
    RealFS realfs;
    if (!realfs.writeFile(destpath, file_buffer.data(), file_buffer.size())) {
        LOG(ERROR) << "Failed to write file: " << destpath;
        
        // Send error acknowledgment
        callback::GenericAck end_ack(callback::AckType::ERROR_RUNTIME_ERROR,
                                     "Failed to write file");
        auto end_ack_packet = createPacket(Command::CMD_GENERIC_ACK, &end_ack,
                                            sizeof(end_ack),
                                            PayloadType::Binary, token);
        backend_->write(end_ack_packet);
        return false;
    }

    // Send success acknowledgment
    callback::GenericAck end_ack = callback::GenericAck::ok();
    auto end_ack_packet = createPacket(Command::CMD_GENERIC_ACK, &end_ack,
                                        sizeof(end_ack), PayloadType::Binary,
                                        token);
    if (!backend_->write(end_ack_packet)) {
        LOG(ERROR) << "Failed to send END acknowledgment";
        return false;
    }

    LOG(INFO) << "File received and saved successfully: " << destpath;
    return true;
}

}  // namespace TgBotSocket::Client