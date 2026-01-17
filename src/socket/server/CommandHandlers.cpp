#include <absl/log/log.h>
#include <fmt/chrono.h>
#include <api/types/ApiException.hpp>
#include <trivial_helpers/_std_chrono_templates.h>

#include <algorithm>
#include <chrono>
#include <fstream>

#include "DataStructParsers.hpp"
#include <shared/PacketParser.hpp>
#include "PacketUtils.hpp"
#include "SocketInterface.hpp"

using namespace TgBotSocket;
using namespace TgBotSocket::callback;

GenericAck SocketInterfaceTgBot::handle_WriteMsgToChatId(
    const std::uint8_t* ptr, TgBotSocket::Packet::Header::length_type len,
    TgBotSocket::PayloadType type) {
    const auto data = WriteMsgToChatId::fromBuffer(ptr, len, type);
    if (!data) {
        return GenericAck(AckType::ERROR_INVALID_ARGUMENT, "Invalid command");
    }
    try {
        api->sendMessage(data->chat, data->message);
    } catch (const api::types::ApiException& e) {
        LOG(ERROR) << "Exception at handler: " << e.what();
        return GenericAck(AckType::ERROR_TGAPI_EXCEPTION, e.what());
    }
    return GenericAck::ok();
}

GenericAck SocketInterfaceTgBot::handle_CtrlSpamBlock(
    const std::uint8_t* ptr) {
    const auto* data =
        reinterpret_cast<const TgBotSocket::data::CtrlSpamBlock*>(ptr);
    spamblock->setConfig(*data);
    return GenericAck::ok();
}

GenericAck SocketInterfaceTgBot::handle_ObserveChatId(
    const std::uint8_t* ptr, TgBotSocket::Packet::Header::length_type len,
    TgBotSocket::PayloadType type) {
    auto data = ObserveChatId::fromBuffer(ptr, len, type);

    if (!data) {
        return GenericAck(AckType::ERROR_INVALID_ARGUMENT, "Invalid command");
    }
    bool observe = data->observe;
    if (!observer->observeAll(true)) {
        return GenericAck(AckType::ERROR_COMMAND_IGNORED,
                          "CMD_OBSERVE_ALL_CHATS active");
    } else {
        observer->observeAll(false);
    }
    if (observe) {
        if (observer->startObserving(data->chat)) {
            LOG(INFO) << "Observing chat '" << data->chat << "'";
        } else {
            return GenericAck(AckType::ERROR_COMMAND_IGNORED,
                              "Target chat was already being observed");
        }
    } else {
        if (observer->stopObserving(data->chat)) {
            LOG(INFO) << "Stopped observing chat '" << data->chat << "'";
        } else {
            return GenericAck(AckType::ERROR_COMMAND_IGNORED,
                              "Target chat wasn't being observed");
        }
    }
    return GenericAck::ok();
}

GenericAck SocketInterfaceTgBot::handle_SendFileToChatId(
    const std::uint8_t* ptr, TgBotSocket::Packet::Header::length_type len,
    TgBotSocket::PayloadType type) {
    const auto data = SendFileToChatId::fromBuffer(ptr, len, type);
    if (!data) {
        return GenericAck(AckType::ERROR_INVALID_ARGUMENT, "Invalid command");
    }
    const auto file = data->filePath.string();
    if (data->filePath.empty()) {
        return GenericAck(AckType::ERROR_INVALID_ARGUMENT, "No file provided");
    }
    std::function<std::optional<api::types::Message>(api::types::Chat::id_type, const TgBotApi::FileOrMedia&)> fn;
    switch (data->fileType) {
        using FileType = TgBotSocket::data::FileType;
        case FileType::TYPE_PHOTO:
            fn = [this](api::types::Chat::id_type id,
                        const TgBotApi::FileOrMedia& file) {
                return api->sendPhoto(id, file);
            };
            break;
        case FileType::TYPE_VIDEO:
            fn = [this](api::types::Chat::id_type id,
                        const TgBotApi::FileOrMedia& file) {
                return api->sendVideo(id, file);
            };
            break;
        case FileType::TYPE_GIF:
            fn = [this](api::types::Chat::id_type id,
                        const TgBotApi::FileOrMedia& file) {
                return api->sendAnimation(id, file);
            };
            break;
        case FileType::TYPE_DOCUMENT:
            fn = [this](api::types::Chat::id_type id,
                        const TgBotApi::FileOrMedia& file) {
                return api->sendDocument(id, file);
            };
            break;
        case FileType::TYPE_STICKER:
            fn = [this](api::types::Chat::id_type id,
                        const TgBotApi::FileOrMedia& file) {
                return api->sendSticker(id, file);
            };
            break;
        case FileType::TYPE_DICE: {
            api->sendDice(data->chat);
            return GenericAck::ok();
        }
        default:
            break;
    }
    DLOG(INFO) << "Sending " << file << " to " << data->chat;
    // Try to send as local file first
    try {
        fn(data->chat,
           api::types::InputFile(file, getMIMEType(resource, file)));
    } catch (const std::ifstream::failure& e) {
        LOG(INFO) << "Failed to send '" << file
                  << "' as local file, trying as Telegram "
                     "file id";
        MediaIds ids{};
        ids.id = file;
        try {
            fn(data->chat, ids);
        } catch (const api::types::ApiException& e) {
            LOG(ERROR) << "Exception at handler, " << e.what();
            return GenericAck(AckType::ERROR_TGAPI_EXCEPTION, e.what());
        }
    }
    return GenericAck::ok();
}

GenericAck SocketInterfaceTgBot::handle_ObserveAllChats(
    const std::uint8_t* ptr, TgBotSocket::Packet::Header::length_type len,
    TgBotSocket::PayloadType type) {
    auto data = ObserveAllChats::fromBuffer(ptr, len, type);
    if (!data) {
        return GenericAck(AckType::ERROR_INVALID_ARGUMENT, "Invalid command");
    }
    observer->observeAll(data->observe);
    return GenericAck::ok();
}

GenericAck SocketInterfaceTgBot::handle_TransferFile(
    const std::uint8_t* ptr, TgBotSocket::Packet::Header::length_type len,
    TgBotSocket::PayloadType type) {
    const auto f = TransferFileMeta::fromBuffer(ptr, len, type);
    if (!f) {
        return GenericAck(AckType::ERROR_INVALID_ARGUMENT,
                          "Cannot parse TransferFileMeta");
    }

    if (!helper->ReceiveTransferMeta(*f)) {
        return GenericAck(AckType::ERROR_COMMAND_IGNORED,
                          "Options verification failed");
    } else {
        return GenericAck::ok();
    }
}

std::optional<Packet> SocketInterfaceTgBot::handle_TransferFileRequest(
    const std::uint8_t* ptr, TgBotSocket::Packet::Header::length_type len,
    const TgBotSocket::Packet::Header::session_token_type& token,
    TgBotSocket::PayloadType type) {
    auto f = TransferFileMeta::fromBuffer(ptr, len, type);
    if (!f) {
        return toJSONPacket(GenericAck(AckType::ERROR_INVALID_ARGUMENT,
                                       "Cannot parse TransferFileMeta"),
                            token);
    }

    // Since a request is made, we need to send the file
    f->options.dry_run = false;

    // Check file size to determine if we should use chunked transfer
    std::error_code ec;
    const auto file_size = std::filesystem::file_size(f->filepath, ec);
    if (ec) {
        LOG(ERROR) << "Failed to get file size for " << f->filepath << ": "
                   << ec.message();
        return toJSONPacket(
            GenericAck(AckType::ERROR_RUNTIME_ERROR,
                       "Failed to get file size: " + ec.message()),
            token);
    }

    // Use chunked transfer for files larger than 10MB
    constexpr uint64_t CHUNKED_TRANSFER_THRESHOLD = 10 * 1024 * 1024;  // 10 MB
    constexpr uint32_t CHUNK_SIZE = 1 * 1024 * 1024;                   // 1 MB chunks

    if (file_size > CHUNKED_TRANSFER_THRESHOLD) {
        LOG(INFO) << "File size " << file_size
                  << " bytes exceeds threshold, using chunked transfer for "
                  << f->filepath;

        // Initiate chunked transfer internally
        return sendFileChunked(f->filepath, f->destfilepath, token, type);
    } else {
        // Use legacy single-packet transfer for small files
        DLOG(INFO) << "File size " << file_size
                   << " bytes, using single-packet transfer";
        return helper->CreateTransferMeta(*f, token, type, false);
    }
}

std::optional<Packet> SocketInterfaceTgBot::handle_GetUptime(
    const TgBotSocket::Packet::Header::session_token_type& token,
    TgBotSocket::PayloadType type) {
    auto now = std::chrono::system_clock::now();
    const auto diff = to_secs(now - startTp);

    switch (type) {
        case PayloadType::Binary: {
            GetUptimeCallback callback{};
            copyTo(callback.uptime, fmt::format("Uptime: {:%H:%M:%S}", diff));
            LOG(INFO) << "Sending text back: "
                      << std::quoted(callback.uptime.data());
            return createPacket(Command::CMD_GET_UPTIME_CALLBACK, &callback,
                                sizeof(callback), PayloadType::Binary, token);
        } break;
        case PayloadType::Json: {
            nlohmann::json payload;
            payload["start_time"] = fmt::format("{}", startTp);
            payload["current_time"] = fmt::format("{}", now);
            payload["uptime"] = fmt::format("{:%Hh %Mm %Ss}", diff);
            LOG(INFO) << "Sending JSON back: " << payload.dump(2);
            return nodeToPacket(Command::CMD_GET_UPTIME_CALLBACK, payload,
                                token);
        } break;
        default:
            LOG(WARNING) << "Unsupported payload type: "
                         << static_cast<int>(type);
    }
    return std::nullopt;
}

std::optional<Packet> SocketInterfaceTgBot::sendFileChunked(
    const std::filesystem::path& srcpath,
    const std::filesystem::path& destpath,
    const TgBotSocket::Packet::Header::session_token_type& token,
    TgBotSocket::PayloadType type) {
    // Read the file
    RealFS realfs;
    auto file_data = realfs.readFile(srcpath);
    if (!file_data) {
        LOG(ERROR) << "Failed to read file: " << srcpath;
        return toJSONPacket(
            GenericAck(AckType::ERROR_RUNTIME_ERROR, "Failed to read file"),
            token);
    }

    // Calculate hash
    HashContainer hash;
    realfs.SHA256(file_data.value(), hash);

    const uint64_t total_size = file_data->size();
    constexpr uint32_t chunk_size = 1 * 1024 * 1024;  // 1 MB chunks

    // Create BEGIN packet
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

    LOG(INFO) << "Starting chunked file transfer: " << srcpath << " -> "
              << destpath << " (" << total_size << " bytes, "
              << ((total_size + chunk_size - 1) / chunk_size) << " chunks)";

    // Return the BEGIN packet as the first response
    // Note: The actual chunk sending will need to be handled by the client
    // in a state machine pattern, or we need to modify the protocol to
    // support server-initiated chunk streaming
    return begin_packet;
}

GenericAck SocketInterfaceTgBot::handle_TransferFileBegin(
    const std::uint8_t* ptr, TgBotSocket::Packet::Header::length_type len,
    const TgBotSocket::Packet::Header::session_token_type& token,
    TgBotSocket::PayloadType type) {
    const auto data = FileTransferBegin::fromBuffer(ptr, len, type);
    if (!data) {
        return GenericAck(AckType::ERROR_INVALID_ARGUMENT,
                          "Invalid FileTransferBegin data");
    }

    std::string session_key(token.data(), token.size());
    std::lock_guard<std::mutex> lock(transfer_sessions_mutex);

    // Check if there's already an active transfer for this session
    if (transfer_sessions.contains(session_key)) {
        return GenericAck(AckType::ERROR_COMMAND_IGNORED,
                          "Transfer session already active");
    }

    // Validate parameters
    if (data->chunk_size == 0 || data->total_size == 0) {
        return GenericAck(AckType::ERROR_INVALID_ARGUMENT,
                          "Invalid chunk_size or total_size");
    }

    if (data->destfilepath.empty()) {
        return GenericAck(AckType::ERROR_INVALID_ARGUMENT,
                          "Empty destination path");
    }

    // Create new transfer session
    ChunkedTransferSession session{
        .destfilepath = data->destfilepath,
        .total_size = data->total_size,
        .chunk_size = data->chunk_size,
        .sha256_hash = data->sha256_hash,
        .buffer = {},
        .expected_chunk_index = 0,
        .start_time = std::chrono::system_clock::now()
    };

    // Reserve buffer space
    session.buffer.reserve(data->total_size);

    transfer_sessions.emplace(session_key, std::move(session));

    LOG(INFO) << "Started chunked transfer session for " << data->destfilepath
              << ", total size: " << data->total_size
              << ", chunk size: " << data->chunk_size;

    return GenericAck::ok();
}

std::optional<Packet> SocketInterfaceTgBot::handle_TransferFileChunk(
    const std::uint8_t* ptr, TgBotSocket::Packet::Header::length_type len,
    const TgBotSocket::Packet::Header::session_token_type& token,
    TgBotSocket::PayloadType type) {
    const auto data = FileTransferChunk::fromBuffer(ptr, len, type);
    if (!data) {
        TgBotSocket::data::FileTransferChunkResponse response{
            .chunk_index = 0,
            .success = false,
        };
        copyTo(response.error_msg, "Invalid FileTransferChunk data");
        return createPacket(Command::CMD_TRANSFER_FILE_CHUNK_RESPONSE,
                            &response, sizeof(response),
                            TgBotSocket::PayloadType::Binary, token);
    }

    std::string session_key(token.data(), token.size());
    std::lock_guard<std::mutex> lock(transfer_sessions_mutex);

    // Find transfer session
    auto it = transfer_sessions.find(session_key);
    if (it == transfer_sessions.end()) {
        TgBotSocket::data::FileTransferChunkResponse response{
            .chunk_index = data->chunk_index,
            .success = false,
        };
        copyTo(response.error_msg, "No active transfer session");
        return createPacket(Command::CMD_TRANSFER_FILE_CHUNK_RESPONSE,
                            &response, sizeof(response),
                            TgBotSocket::PayloadType::Binary, token);
    }

    auto& session = it->second;

    // Verify chunk index
    if (data->chunk_index != session.expected_chunk_index) {
        TgBotSocket::data::FileTransferChunkResponse response{
            .chunk_index = data->chunk_index,
            .success = false,
        };
        copyTo(response.error_msg,
               fmt::format("Expected chunk {}, got {}",
                           session.expected_chunk_index, data->chunk_index));
        LOG(WARNING) << "Chunk index mismatch for "
                     << session.destfilepath.string();
        return createPacket(Command::CMD_TRANSFER_FILE_CHUNK_RESPONSE,
                            &response, sizeof(response),
                            TgBotSocket::PayloadType::Binary, token);
    }

    // Verify we don't exceed total size
    if (session.buffer.size() + data->chunk_data_size > session.total_size) {
        TgBotSocket::data::FileTransferChunkResponse response{
            .chunk_index = data->chunk_index,
            .success = false,
        };
        copyTo(response.error_msg, "Chunk would exceed total file size");
        LOG(ERROR) << "Chunk size overflow for "
                   << session.destfilepath.string();
        
        // Clean up failed session
        transfer_sessions.erase(it);
        
        return createPacket(Command::CMD_TRANSFER_FILE_CHUNK_RESPONSE,
                            &response, sizeof(response),
                            TgBotSocket::PayloadType::Binary, token);
    }

    // Append chunk data to buffer
    session.buffer.insert(session.buffer.end(), data->chunk_data,
                          data->chunk_data + data->chunk_data_size);
    session.expected_chunk_index++;

    DLOG(INFO) << "Received chunk " << data->chunk_index << " for "
               << session.destfilepath.string() << " ("
               << session.buffer.size() << "/" << session.total_size << " bytes)";

    // Send success response
    TgBotSocket::data::FileTransferChunkResponse response{
        .chunk_index = data->chunk_index,
        .success = true,
    };
    copyTo(response.error_msg, "OK");

    return createPacket(Command::CMD_TRANSFER_FILE_CHUNK_RESPONSE, &response,
                        sizeof(response), TgBotSocket::PayloadType::Binary,
                        token);
}

GenericAck SocketInterfaceTgBot::handle_TransferFileEnd(
    const std::uint8_t* ptr, TgBotSocket::Packet::Header::length_type len,
    const TgBotSocket::Packet::Header::session_token_type& token,
    TgBotSocket::PayloadType type) {
    const auto data = FileTransferEnd::fromBuffer(ptr, len, type);
    if (!data) {
        return GenericAck(AckType::ERROR_INVALID_ARGUMENT,
                          "Invalid FileTransferEnd data");
    }

    std::string session_key(token.data(), token.size());
    std::lock_guard<std::mutex> lock(transfer_sessions_mutex);

    // Find transfer session
    auto it = transfer_sessions.find(session_key);
    if (it == transfer_sessions.end()) {
        return GenericAck(AckType::ERROR_COMMAND_IGNORED,
                          "No active transfer session");
    }

    auto& session = it->second;

    // Verify we received all data
    if (session.buffer.size() != session.total_size) {
        auto error_msg = fmt::format(
            "Incomplete transfer: received {} bytes, expected {} bytes",
            session.buffer.size(), session.total_size);
        LOG(ERROR) << error_msg;
        transfer_sessions.erase(it);
        return GenericAck(AckType::ERROR_RUNTIME_ERROR, error_msg);
    }

    // Verify hash if requested
    if (data->verify_hash) {
        HashContainer computed_hash{};
        SharedMalloc file_mem(session.buffer.size());
        file_mem.assignFrom(session.buffer.data(), session.buffer.size());
        
        // Use VFS to compute hash
        RealFS realfs;
        realfs.SHA256(file_mem, computed_hash);

        if (!std::ranges::equal(computed_hash.m_data, session.sha256_hash)) {
            LOG(ERROR) << "Hash verification failed for "
                       << session.destfilepath.string();
            transfer_sessions.erase(it);
            return GenericAck(AckType::ERROR_RUNTIME_ERROR,
                              "Hash verification failed");
        }
        DLOG(INFO) << "Hash verification successful";
    }

    // Write file to disk
    RealFS realfs;
    if (!realfs.writeFile(session.destfilepath, session.buffer.data(),
                          session.buffer.size())) {
        LOG(ERROR) << "Failed to write file: " << session.destfilepath.string();
        transfer_sessions.erase(it);
        return GenericAck(AckType::ERROR_RUNTIME_ERROR, "Failed to write file");
    }

    auto duration = std::chrono::system_clock::now() - session.start_time;
    auto duration_sec = std::chrono::duration_cast<std::chrono::seconds>(duration).count();
    
    LOG(INFO) << "Completed chunked transfer for "
              << session.destfilepath.string() << " (" << session.total_size
              << " bytes) in " << duration_sec << " seconds";

    // Clean up session
    transfer_sessions.erase(it);

    return GenericAck::ok();
}