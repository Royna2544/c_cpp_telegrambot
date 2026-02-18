#include "SocketServiceImpl.hpp"

#include <absl/hash/hash.h>
#include <absl/log/log.h>
#include <absl/strings/strip.h>
#include <fmt/format.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/support/status.h>
#include <uuid.h>

#include <GitBuildInfo.hpp>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <random>
#include <string_view>
#include <system_error>

#include "Socket_service.grpc.pb.h"
#include "Socket_service.pb.h"
#include "api/TgBotApi.hpp"
#include "api/Utils.hpp"
#include "global_handlers/SpamBlock.hpp"
#include "hex.h"
#include "sha.h"
#include "tgbot/TgException.h"

#define CRYPTOPP_ENABLE_NAMESPACE_WEAK 1
#include <cryptopp/blake2.h>
#include <cryptopp/cryptlib.h>
#include <cryptopp/hex.h>
#include <cryptopp/md5.h>
#include <cryptopp/sha.h>

using grpc::ServerContext;
using grpc::Status;
using namespace tgbot::proto::socket;

class SocketServiceImpl::Service : public SocketService::Service {
   public:
    explicit Service(TgBotApi* api, SpamBlockBase* spamBlock,
                     DatabaseBase* database)
        : api_(api), spamBlock_(spamBlock), database_(database) {}

    std::chrono::system_clock::time_point startTime_ =
        std::chrono::system_clock::now();

    Status sendMessage(ServerContext* context,
                       const SendMessageRequest* request,
                       GenericResponse* response) override;
    Status setSpamBlockingConfig(ServerContext* context,
                                 const SpamBlockingConfig* request,
                                 GenericResponse* response) override;
    Status getSpamBlockingConfig(ServerContext* context,
                                 const ::google::protobuf::Empty* request,
                                 SpamBlockingConfig* response) override;
    Status requestFileTransfer(ServerContext* context,
                               const FileTransferRequest* request,
                               FileTransferResponse* response) override;
    Status downloadFileLoop(
        ServerContext* context,
        ::grpc::ServerReaderWriter<FileChunkResponse, FileChunkRequest>* stream)
        override;
    Status uploadFileLoop(
        ServerContext* context,
        ::grpc::ServerReaderWriter<FileChunkResponse, FileChunk>* stream)
        override;
    Status endFileTransfer(ServerContext* context,
                           const FileTransferRequest* request,
                           GenericResponse* response) override;
    Status ping(ServerContext* context,
                const ::google::protobuf::Empty* request,
                ::google::protobuf::Empty* response) override;
    Status info(ServerContext* context,
                const ::google::protobuf::Empty* request,
                BotInfo* response) override;

    Status setChatAlias(ServerContext* context, const ChatAlias* request,
                        GenericResponse* response) override;
    Status getChatAlias(ServerContext* context, const ChatAliasRequest* request,
                        ChatAliasResponse* response) override;
    Status deleteChatAlias(ServerContext* context,
                           const ChatAliasRequest* request,
                           GenericResponse* response) override;
    Status listChatAliases(ServerContext* context,
                           const ::google::protobuf::Empty* request,
                           ::grpc::ServerWriter<ChatAlias>* writer) override;
    Status setMediaAlias(ServerContext* context, const MediaAlias* request,
                         GenericResponse* response) override;
    Status getMediaAlias(ServerContext* context,
                         const MediaAliasRequest* request,
                         MediaAliasResponse* response) override;
    Status deleteMediaAlias(ServerContext* context,
                            const MediaAliasRequest* request,
                            GenericResponse* response) override;
    Status listMediaAliases(ServerContext* context,
                            const ::google::protobuf::Empty* request,
                            ::grpc::ServerWriter<MediaAlias>* writer) override;

   private:
    TgBotApi* api_;
    SpamBlockBase* spamBlock_;
    DatabaseBase* database_;

    struct TranferEntry {
        std::fstream fileStream;
        std::filesystem::path filePath;
        std::uintmax_t totalSize = 0;
        int chunk_count{};
    };

    std::map<std::string, TranferEntry> activeTransfers_;
    std::mutex activeTransfersMutex_;
};

namespace {
std::optional<TgBotApi::FileOrMedia> makeFileOrMedia(
    const SendMessageRequest* request, GenericResponse* response) {
    TgBotApi::FileOrMedia fileOrMedia;

    // Guess the MIME type based on file type
    std::string fileType;
    switch (request->file_type()) {
        case FileType::PHOTO:
            fileType = "image/png";
            break;
        case FileType::VIDEO:
            fileType = "video/mp4";
            break;
        case FileType::DOCUMENT:
            fileType = "application/octet-stream";
            break;
        // At the moment we do not support GIFs via local sending.
        case FileType::GIF:
        // Sticker and dice does not make sense to be sent from local
        // files.
        case FileType::STICKER:
        case FileType::DICE:
        default:
            // Meanwhile, handle unsupported-for-local-file here.
            if (!request->has_file_id()) {
                response->set_code(GenericResponseCode::ErrorCommandIgnored);
                response->set_message(
                    "Unsupported file type for local sending");
                return std::nullopt;
            }
            break;
    }

    if (request->has_file_path()) {
        // 1. If it has file path.
        try {
            fileOrMedia = InputFile::fromFile(request->file_path(), fileType);
        } catch (const std::ifstream::failure& e) {
            response->set_code(GenericResponseCode::ErrorCommandIgnored);
            response->set_message(
                fmt::format("Failed to read file: {}", e.what()));
            return std::nullopt;
        }
    } else if (request->has_file_data()) {
        // 2. If it has file data.
        auto inputFile = std::make_shared<InputFile>();
        inputFile->data = request->file_data();
        inputFile->mimeType = fileType;
        inputFile->fileName = "upload_file";  // Default file name
        fileOrMedia = inputFile;
    } else if (request->has_file_id()) {
        // 3. If it has file ID. (We do not know uniqueid but we do not care)
        fileOrMedia = MediaIds{request->file_id(), ""};
    } else {
        response->set_code(GenericResponseCode::ErrorCommandIgnored);
        response->set_message("No file data, path, or ID provided");
        return std::nullopt;
    }
    return fileOrMedia;
}

void LogWhoCalledMe(ServerContext* context, const std::string& methodName) {
    auto peer = context->peer();
    std::string_view unused = peer;
    if (absl::ConsumePrefix(&unused, "ipv4:127.0.0.1")) {
        // Localhost call, ignore logging
        DLOG(INFO) << "Localhost call to " << methodName;
        return;
    }
    LOG(INFO) << "Method " << methodName << " called by " << peer;
}

}  // namespace

Status SocketServiceImpl::Service::sendMessage(
    ServerContext* context, const SendMessageRequest* request,
    GenericResponse* response) {
    LogWhoCalledMe(context, "sendMessage");

    // 1. Check if file_type,text is present
    bool hasFileType = request->has_file_type();
    bool hasText = request->has_text();

    // 2. Check if file_type or text is provided
    if (!hasFileType && !hasText) {
        response->set_code(GenericResponseCode::ErrorCommandIgnored);
        response->set_message("Either file_type or text must be provided");
        return Status::OK;
    }

    // 3. Store file if needed
    TgBotApi::FileOrMedia fileOrMedia;
    if (hasFileType) {
        auto fileOrMediaOpt = makeFileOrMedia(request, response);
        if (!fileOrMediaOpt.has_value()) {
            return Status::OK;
        }
        fileOrMedia = *fileOrMediaOpt;
    }

    // 4. store text if needed
    constexpr std::string_view emptyStringView;
    std::string_view textOpt = hasText ? request->text() : emptyStringView;

    // 5. Process the request based on file_type presence
    try {
        if (hasFileType) {
            switch (request->file_type()) {
                case FileType::PHOTO:
                    api_->sendPhoto(request->chat_id(), fileOrMedia, textOpt);
                    break;
                case FileType::VIDEO:
                    api_->sendVideo(request->chat_id(), fileOrMedia, textOpt);
                    break;
                case FileType::DOCUMENT:
                    api_->sendDocument(request->chat_id(), fileOrMedia,
                                       textOpt);
                    break;
                case FileType::GIF:
                    api_->sendAnimation(request->chat_id(), fileOrMedia,
                                        textOpt);
                    break;
                case FileType::STICKER:
                    api_->sendSticker(request->chat_id(), fileOrMedia);
                    break;
                case FileType::DICE:
                    api_->sendDice(request->chat_id());
                    break;
                default:
                    response->set_code(
                        GenericResponseCode::ErrorCommandIgnored);
                    response->set_message("Unsupported file type");
                    return Status::OK;
            }
        } else if (hasText) {
            api_->sendMessage(request->chat_id(), textOpt);
        }
    } catch (const TgBot::TgException& ex) {
        response->set_code(GenericResponseCode::TelegramApiException);
        response->set_message(
            fmt::format("Failed to send message: {}", ex.what()));
        return Status::OK;
    }

    response->set_code(GenericResponseCode::Success);
    response->set_message("Message sent successfully");
    return Status::OK;
}

Status SocketServiceImpl::Service::setSpamBlockingConfig(
    ServerContext* context, const SpamBlockingConfig* request,
    GenericResponse* response) {
    LogWhoCalledMe(context, "setSpamBlockingConfig");

    LOG(INFO) << "Setting spam blocking config mode";
    spamBlock_->setConfig(static_cast<SpamBlockBase::Config>(request->mode()));
    response->set_code(GenericResponseCode::Success);
    response->set_message("Spam blocking config updated successfully");
    return Status::OK;
}

Status SocketServiceImpl::Service::getSpamBlockingConfig(
    ServerContext* context, const ::google::protobuf::Empty* /*request*/,
    SpamBlockingConfig* response) {
    LogWhoCalledMe(context, "getSpamBlockingConfig");

    SpamBlockBase::Config config = spamBlock_->getConfig();
    response->set_mode(static_cast<SpamBlockingModes>(config));
    return Status::OK;
}

namespace {
template <typename T>
std::string calc_hash(const std::string& data) {
    T hash_obj;
    hash_obj.Update(reinterpret_cast<const CryptoPP::byte*>(data.data()),
                    data.size());
    std::string digest;
    digest.resize(hash_obj.DigestSize());
    hash_obj.Final(reinterpret_cast<CryptoPP::byte*>(digest.data()));
    std::string encoded;
    CryptoPP::HexEncoder encoder;
    encoder.Attach(new CryptoPP::StringSink(encoded));
    encoder.Put(reinterpret_cast<const CryptoPP::byte*>(digest.data()),
                digest.size());
    encoder.MessageEnd();
    return encoded;
}

}  // namespace
Status SocketServiceImpl::Service::requestFileTransfer(
    ServerContext* context, const FileTransferRequest* request,
    FileTransferResponse* response) {
    LogWhoCalledMe(context, "requestFileTransfer");

    LOG(INFO) << "Received file transfer request for path: "
              << request->file_path()
              << (request->is_upload() ? " (upload)" : " (download)");

    // 1. Create UUID for the transfer
    std::mt19937 rng(std::random_device{}());
    uuids::uuid uuid = uuids::uuid_random_generator{rng}();
    response->set_uuid(uuids::to_string(uuid));
    // 2. Create file stream and store in activeTransfers_
    TranferEntry entry;

    {
        std::lock_guard<std::mutex> lock(activeTransfersMutex_);
        if (activeTransfers_.contains(uuids::to_string(uuid))) {
            response->set_accepted(false);
            LOG(ERROR) << "File transfer with UUID already exists: "
                       << uuids::to_string(uuid);
            return Status::OK;
        }
    }

    std::error_code ec;
    if (request->is_upload()) {
        // Check: is file already there?
        if (!request->overwrite_existing() &&
            std::filesystem::exists(request->file_path(), ec)) {
            response->set_accepted(false);
            response->set_reject_message("File already exists for upload");
            LOG(ERROR) << "File already exists for upload: "
                       << request->file_path();
            return Status::OK;
        }

        // Compute checksums if set
        std::optional<std::string> result;
        switch (request->file_checksum_algorithm()) {
            case ChecksumAlgorithm::MD5: {
                result = calc_hash<CryptoPP::Weak::MD5>(request->file_checksum());
            } break;
            case ChecksumAlgorithm::SHA1: {
                result = calc_hash<CryptoPP::SHA1>(request->file_checksum());
            } break;
            case ChecksumAlgorithm::SHA256: {
                result = calc_hash<CryptoPP::SHA256>(request->file_checksum());
            } break;
            case ChecksumAlgorithm::SHA512: {
                result = calc_hash<CryptoPP::SHA512>(request->file_checksum());
            } break;
            case ChecksumAlgorithm::Blake2b: {
                result = calc_hash<CryptoPP::BLAKE2b>(request->file_checksum());
            } break;
            case ChecksumAlgorithm::Blake2s: {
                result = calc_hash<CryptoPP::BLAKE2s>(request->file_checksum());
            } break;
            case ChecksumAlgorithm::None:
                break;
        }
        if (result.has_value()) {
            std::string providedChecksum = request->file_checksum();
            if (absl::Hash<std::string_view>{}(*result) !=
                absl::Hash<std::string_view>{}(providedChecksum)) {
                response->set_accepted(false);
                response->set_reject_message("Checksum mismatch");
                LOG(ERROR) << "Checksum mismatch for upload. Provided: "
                           << std::quoted(providedChecksum)
                           << ", Computed: " << std::quoted(*result);
                return Status::OK;
            }
            LOG(INFO) << "Checksum verified for upload: "
                      << std::quoted(providedChecksum);
        }

        // For upload, we create a temporary file to store incoming data
        entry.filePath = std::filesystem::temp_directory_path() /
                         fmt::format("upload_{}.tmp", uuids::to_string(uuid));

        entry.totalSize = request->file_size();
        // Create the file first, then resize
        entry.fileStream.open(entry.filePath, std::ios::binary | std::ios::out);
        entry.fileStream.close();
        std::filesystem::resize_file(entry.filePath, entry.totalSize, ec);

        LOG(INFO) << "Resized file: " << entry.filePath.string() << " to size "
                  << entry.totalSize << " bytes";
        entry.fileStream.open(entry.filePath, std::ios::binary | std::ios::out);
        if (!entry.fileStream.is_open()) {
            response->set_accepted(false);
            response->set_reject_message("Failed to create temporary file");
            LOG(ERROR) << "Failed to create temporary file for upload";
            return Status::OK;
        }
    } else {
        // For download, we open the requested file
        entry.filePath = request->file_path();
        entry.fileStream.open(entry.filePath, std::ios::binary | std::ios::in);
        if (!entry.fileStream.is_open()) {
            response->set_accepted(false);
            response->set_reject_message("Failed to open file for download");
            LOG(ERROR) << "Failed to open file for download: "
                       << entry.filePath.string();
            return Status::OK;
        }
        entry.totalSize = std::filesystem::file_size(entry.filePath);
        // Calculate chunk_count
        constexpr std::uintmax_t chunkSize = 64 * 1024;  // 64 KB
        entry.chunk_count =
            static_cast<int>((entry.totalSize + chunkSize - 1) / chunkSize);
        response->set_file_size(entry.totalSize);
        response->set_chunk_count(entry.chunk_count);
    }
    {
        std::lock_guard<std::mutex> lock(activeTransfersMutex_);
        activeTransfers_.emplace(uuids::to_string(uuid), std::move(entry));
    }
    response->set_accepted(true);
    return Status::OK;
}

Status SocketServiceImpl::Service::downloadFileLoop(
    ServerContext* context,
    ::grpc::ServerReaderWriter<FileChunkResponse, FileChunkRequest>* stream) {
    FileChunkRequest msg;
    constexpr std::uintmax_t CHUNK_SIZE = 64 * 1024;  // 64 KB

    LogWhoCalledMe(context, "downloadFileLoop");

    while (stream->Read(&msg)) {
        std::lock_guard<std::mutex> lock(activeTransfersMutex_);
        if (!activeTransfers_.contains(msg.uuid())) {
            // Invalid UUID
            FileChunkResponse response;
            response.set_success(false);
            response.set_retry(false);
            LOG(ERROR) << "Invalid UUID for file download: "
                       << std::quoted(msg.uuid());
            stream->Write(response);
            return Status::OK;
        }

        std::fstream& fileStream = activeTransfers_[msg.uuid()].fileStream;

        fileStream.seekg(msg.chunk_idx() * CHUNK_SIZE, std::ios::beg);
        if (!fileStream.good()) {
            // Seek failed
            FileChunkResponse response;
            response.set_success(false);
            response.set_retry(true);
            LOG(ERROR) << "Seek failed during file download: " << msg.uuid();
            stream->Write(response);
            return Status::OK;
        }

        std::vector<char> buffer(CHUNK_SIZE);
        fileStream.read(buffer.data(), buffer.size());
        std::streamsize bytesRead = fileStream.gcount();

        if (bytesRead <= 0) {
            // Read failed
            FileChunkResponse response;
            response.set_success(false);
            response.set_retry(true);
            LOG(ERROR) << "Read failed during file download: " << msg.uuid();
            stream->Write(response);
            return Status::OK;
        }

        // Send chunk data
        FileChunkResponse response;
        response.set_success(true);
        response.set_retry(false);
        response.mutable_chunk()->set_chunk_idx(msg.chunk_idx());
        response.mutable_chunk()->set_chunk_data(
            buffer.data(), static_cast<size_t>(bytesRead));
        stream->Write(response);
        LOG_EVERY_N_SEC(INFO, 10)
            << "Sent chunk " << msg.chunk_idx() << " for download UUID "
            << std::quoted(msg.uuid());
    }

    return Status::OK;
}

Status SocketServiceImpl::Service::uploadFileLoop(
    ServerContext* context,
    ::grpc::ServerReaderWriter<FileChunkResponse, FileChunk>* stream) {
    FileChunk msg;

    LogWhoCalledMe(context, "uploadFileLoop");
    while (stream->Read(&msg)) {
        std::lock_guard<std::mutex> lock(activeTransfersMutex_);
        if (!activeTransfers_.contains(msg.uuid())) {
            // Invalid UUID
            FileChunkResponse response;
            response.set_success(false);
            response.set_retry(false);
            LOG(ERROR) << "Invalid UUID for file upload: "
                       << std::quoted(msg.uuid());
            stream->Write(response);
            return Status::OK;
        }

        std::fstream& fileStream = activeTransfers_[msg.uuid()].fileStream;

        fileStream.seekp(msg.chunk_offset(), std::ios::beg);
        if (!fileStream.good()) {
            // Seek failed
            FileChunkResponse response;
            response.set_success(false);
            response.set_retry(true);
            LOG(ERROR) << "Seek failed during file upload: " << msg.uuid();
            stream->Write(response);
            return Status::OK;
        }
        fileStream.write(msg.chunk_data().data(), msg.chunk_data().size());
        if (!fileStream.good()) {
            // Write failed
            FileChunkResponse response;
            response.set_success(false);
            response.set_retry(true);
            LOG(ERROR) << "Write failed during file upload: " << msg.uuid();
            stream->Write(response);
            return Status::OK;
        }
        // Send success response
        FileChunkResponse response;
        response.set_success(true);
        response.set_retry(false);
        stream->Write(response);
        LOG_EVERY_N_SEC(INFO, 10)
            << "Received chunk " << msg.chunk_idx() << " for upload UUID "
            << std::quoted(msg.uuid());
    }
    return Status::OK;
}

Status SocketServiceImpl::Service::endFileTransfer(
    ServerContext* context, const FileTransferRequest* request,
    GenericResponse* response) {
    LogWhoCalledMe(context, "endFileTransfer");
    std::lock_guard<std::mutex> lock(activeTransfersMutex_);
    auto it = activeTransfers_.find(request->uuid());
    if (it == activeTransfers_.end()) {
        response->set_code(GenericResponseCode::ErrorCommandIgnored);
        response->set_message("Invalid UUID for ending file transfer");
        LOG(ERROR) << "Invalid UUID for ending file transfer: "
                   << request->uuid();
        return Status::OK;
    }
    TranferEntry& entry = it->second;
    entry.fileStream.close();
    if (request->is_upload()) {
        // Move temporary file to final destination
        try {
            std::filesystem::copy_file(entry.filePath, request->file_path());
            std::filesystem::remove(entry.filePath);
            LOG(INFO) << "File uploaded successfully to: "
                      << request->file_path();
        } catch (const std::filesystem::filesystem_error& e) {
            response->set_code(GenericResponseCode::ErrorCommandIgnored);
            response->set_message(
                fmt::format("Failed to move uploaded file: {}", e.what()));
            LOG(ERROR) << "Failed to move uploaded file: " << e.what();
            // Clean up temporary file
            std::filesystem::remove(entry.filePath);
            activeTransfers_.erase(it);
            return Status::OK;
        }
    }
    // Remove from active transfers
    activeTransfers_.erase(it);
    response->set_code(GenericResponseCode::Success);
    response->set_message("File transfer ended successfully");
    LOG(INFO) << "File transfer ended successfully: " << request->uuid();
    return Status::OK;
}

Status SocketServiceImpl::Service::ping(
    ServerContext* context, const ::google::protobuf::Empty* /*request*/,
    ::google::protobuf::Empty* /*response*/) {
    LogWhoCalledMe(context, "ping");
    LOG(INFO) << "Pong!";
    return Status::OK;
}

Status SocketServiceImpl::Service::info(
    ServerContext* context, const ::google::protobuf::Empty* /*request*/,
    BotInfo* response) {
    LogWhoCalledMe(context, "info");

    auto botInfo = api_->getBotUser();
    response->set_user_id(botInfo->id);
    if (botInfo->username.has_value()) {
        response->set_username(*botInfo->username);
    }
    response->set_operating_system(buildinfo::OS);
    auto now = std::chrono::system_clock::now();
    auto uptimeDuration = now - startTime_;

    auto dp =
        std::chrono::floor<std::chrono::days>(uptimeDuration);  // Date part
    std::chrono::hh_mm_ss hms{uptimeDuration - dp};             // Time part

    Uptime* uptime = response->mutable_uptime();
    uptime->set_seconds(static_cast<int32_t>(hms.seconds().count()));
    uptime->set_minutes(static_cast<int32_t>(hms.minutes().count()));
    uptime->set_hours(static_cast<int32_t>(hms.hours().count()));
    uptime->set_days(static_cast<int32_t>(
        std::chrono::duration_cast<std::chrono::days>(uptimeDuration).count()));
    return Status::OK;
}

Status SocketServiceImpl::Service::setChatAlias(ServerContext* context,
                                                const ChatAlias* request,
                                                GenericResponse* response) {
    LogWhoCalledMe(context, "setChatAlias");

    switch (database_->addChatInfo(request->chat_id(), request->alias())) {
        case DatabaseBase::AddResult::OK:
            break;
        case DatabaseBase::AddResult::ALREADY_EXISTS:
            response->set_code(GenericResponseCode::ErrorCommandIgnored);
            response->set_message("Failed to set chat alias: already exists");
            return Status::OK;
        case DatabaseBase::AddResult::BACKEND_ERROR:
            response->set_code(GenericResponseCode::ErrorRuntimeError);
            response->set_message("Failed to set chat alias: backend error");
            return Status::OK;
    }
    response->set_code(GenericResponseCode::Success);
    response->set_message("Chat alias set successfully");
    return Status::OK;
}

Status SocketServiceImpl::Service::getChatAlias(ServerContext* context,
                                                const ChatAliasRequest* request,
                                                ChatAliasResponse* response) {
    LogWhoCalledMe(context, "getChatAlias");

    auto chatInfoOpt = database_->getChatId(request->alias());
    if (!chatInfoOpt.has_value()) {
        response->set_exists(false);
        response->set_chat_id(-1);
        return Status::OK;
    }
    response->set_exists(true);
    response->set_chat_id(*chatInfoOpt);
    LOG(INFO) << "Retrieved chat alias for chat_id_alias " << request->alias()
              << ": " << *chatInfoOpt;
    return Status::OK;
}

Status SocketServiceImpl::Service::deleteChatAlias(
    ServerContext* context, const ChatAliasRequest* request,
    GenericResponse* response) {
    LogWhoCalledMe(context, "deleteChatAlias");

    if (!database_->deleteChatInfo(request->chat_id())) {
        response->set_code(GenericResponseCode::ErrorCommandIgnored);
        response->set_message("Failed to delete chat alias: not found");
        return Status::OK;
    }
    response->set_code(GenericResponseCode::Success);
    response->set_message("Chat alias deleted successfully");
    return Status::OK;
}

Status SocketServiceImpl::Service::listChatAliases(
    ServerContext* context, const ::google::protobuf::Empty* /*request*/,
    ::grpc::ServerWriter<ChatAlias>* writer) {
    LogWhoCalledMe(context, "listChatAliases");

    auto chatInfos = database_->getAllChatInfos();
    for (const auto& chatInfo : chatInfos) {
        ChatAlias alias;
        alias.set_chat_id(chatInfo.chatId);
        alias.set_alias(chatInfo.name);
        writer->Write(alias);
    }
    return Status::OK;
}

Status SocketServiceImpl::Service::setMediaAlias(ServerContext* context,
                                                 const MediaAlias* request,
                                                 GenericResponse* response) {
    LogWhoCalledMe(context, "setMediaAlias");

    DatabaseBase::MediaInfo mediaInfo;
    mediaInfo.mediaId = request->media_id();
    mediaInfo.mediaUniqueId = request->media_unique_id();
    mediaInfo.mediaType =
        static_cast<DatabaseBase::MediaType>(request->media_type());
    std::ranges::transform(request->alias(),
                           std::back_inserter(mediaInfo.names),
                           [](const std::string& alias) { return alias; });

    switch (database_->addMediaInfo(mediaInfo)) {
        case DatabaseBase::AddResult::OK:
            break;
        case DatabaseBase::AddResult::ALREADY_EXISTS:
            response->set_code(GenericResponseCode::ErrorCommandIgnored);
            response->set_message("Failed to set media alias: already exists");
            return Status::OK;
        case DatabaseBase::AddResult::BACKEND_ERROR:
            response->set_code(GenericResponseCode::ErrorRuntimeError);
            response->set_message("Failed to set media alias: backend error");
            return Status::OK;
    }
    response->set_code(GenericResponseCode::Success);
    response->set_message("Media alias set successfully");
    return Status::OK;
}

Status SocketServiceImpl::Service::listMediaAliases(
    ServerContext* context, const ::google::protobuf::Empty* /*request*/,
    ::grpc::ServerWriter<MediaAlias>* writer) {
    LogWhoCalledMe(context, "listMediaAliases");
    auto mediaInfos = database_->getAllMediaInfos();
    for (const auto& mediaInfo : mediaInfos) {
        MediaAlias alias;
        alias.set_media_id(mediaInfo.mediaId);
        alias.set_media_unique_id(mediaInfo.mediaUniqueId);
        for (const auto& name : mediaInfo.names) {
            alias.add_alias(name);
        }
        writer->Write(alias);
    }
    return Status::OK;
}

Status SocketServiceImpl::Service::deleteMediaAlias(
    ServerContext* context, const MediaAliasRequest* request,
    GenericResponse* response) {
    LogWhoCalledMe(context, "deleteMediaAlias");

    if (!database_->deleteMediaInfo(request->media_id())) {
        response->set_code(GenericResponseCode::ErrorCommandIgnored);
        response->set_message("Failed to delete media alias: not found");
        return Status::OK;
    }
    response->set_code(GenericResponseCode::Success);
    response->set_message("Media alias deleted successfully");
    return Status::OK;
}

Status SocketServiceImpl::Service::getMediaAlias(
    ServerContext* context, const MediaAliasRequest* request,
    MediaAliasResponse* response) {
    LogWhoCalledMe(context, "getMediaAlias");

    auto mediaInfoOpt = database_->getMediaIds(request->alias());
    if (!mediaInfoOpt.has_value()) {
        response->set_exists(false);
        return Status::OK;
    }
    response->set_exists(true);
    for (const auto& mediaId : *mediaInfoOpt) {
        response->add_media_id(mediaId);
    }
    LOG(INFO) << "Retrieved media alias for media_id_alias " << request->alias()
              << ": " << mediaInfoOpt->size() << " media IDs";
    return Status::OK;
}

struct SocketServiceImpl::Impl {
    std::unique_ptr<grpc::Server> server;
};

void SocketServiceImpl::runFunction(const std::stop_token& token) {
    grpc::ServerBuilder builder;
    builder.AddListeningPort(url_->url, grpc::InsecureServerCredentials());
    builder.RegisterService(service_.get());
    impl_->server = builder.BuildAndStart();
    if (!impl_->server) {
        LOG(ERROR) << "Failed to start gRPC server for SocketServiceImpl";
        return;
    }
    LOG(INFO) << "SocketServiceImpl::Run: " << url_->url;
    impl_->server->Wait();
}

void SocketServiceImpl::onPreStop() { impl_->server->Shutdown(); }

SocketServiceImpl::SocketServiceImpl(TgBotApi* api, SpamBlockBase* spamBlock,
                                     Url* url, DatabaseBase* database)
    : api_(api), spamBlock_(spamBlock), url_(url), database_(database) {
    service_ = std::make_unique<Service>(api, spamBlock, database);
    impl_ = std::make_unique<Impl>();
}

SocketServiceImpl::~SocketServiceImpl() = default;