#include "SocketServiceImpl.hpp"

#include <absl/log/log.h>
#include <absl/strings/ascii.h>
#include <absl/strings/strip.h>
#include <fmt/format.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/support/status.h>
#include <uuid.h>

#include <GitBuildInfo.hpp>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <optional>
#include <random>
#include <string_view>
#include <system_error>
#include <vector>

#include "Socket_service.grpc.pb.h"
#include "Socket_service.pb.h"
#include "api/TgBotApi.hpp"
#include "api/Utils.hpp"
#include "global_handlers/SpamBlock.hpp"
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
        std::filesystem::path destinationPath;
        std::uintmax_t totalSize = 0;
        int chunk_count{};
        ChecksumAlgorithm checksumAlgorithm = ChecksumAlgorithm::None;
        std::string checksum;
        bool overwriteExisting = false;
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
std::string hex_encode(std::string_view data) {
    std::string encoded;
    CryptoPP::HexEncoder encoder;
    encoder.Attach(new CryptoPP::StringSink(encoded));
    encoder.Put(reinterpret_cast<const CryptoPP::byte*>(data.data()),
                data.size());
    encoder.MessageEnd();
    return encoded;
}

template <typename T>
std::optional<std::string> calc_file_hash(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return std::nullopt;
    }

    T hash_obj;
    std::array<char, 64 * 1024> buffer{};
    while (input.read(buffer.data(), buffer.size()) || input.gcount() > 0) {
        hash_obj.Update(reinterpret_cast<const CryptoPP::byte*>(buffer.data()),
                        static_cast<size_t>(input.gcount()));
    }

    std::string digest(hash_obj.DigestSize(), '\0');
    hash_obj.Final(reinterpret_cast<CryptoPP::byte*>(digest.data()));
    return hex_encode(digest);
}

template <typename T>
bool file_checksum_matches(const std::filesystem::path& path,
                           std::string providedChecksum) {
    auto calculatedChecksum = calc_file_hash<T>(path);
    if (!calculatedChecksum.has_value()) {
        return false;
    }

    T hash_obj;
    if (providedChecksum.size() == hash_obj.DigestSize()) {
        providedChecksum = hex_encode(providedChecksum);
    } else {
        providedChecksum = absl::AsciiStrToUpper(providedChecksum);
    }
    return providedChecksum == *calculatedChecksum;
}

bool verify_file_checksum(const std::filesystem::path& path,
                          ChecksumAlgorithm algorithm,
                          const std::string& providedChecksum) {
    switch (algorithm) {
        case ChecksumAlgorithm::MD5:
            return file_checksum_matches<CryptoPP::Weak::MD5>(
                path, providedChecksum);
        case ChecksumAlgorithm::SHA1:
            return file_checksum_matches<CryptoPP::SHA1>(path,
                                                         providedChecksum);
        case ChecksumAlgorithm::SHA256:
            return file_checksum_matches<CryptoPP::SHA256>(path,
                                                           providedChecksum);
        case ChecksumAlgorithm::SHA512:
            return file_checksum_matches<CryptoPP::SHA512>(path,
                                                           providedChecksum);
        case ChecksumAlgorithm::Blake2b:
            return file_checksum_matches<CryptoPP::BLAKE2b>(path,
                                                            providedChecksum);
        case ChecksumAlgorithm::Blake2s:
            return file_checksum_matches<CryptoPP::BLAKE2s>(path,
                                                            providedChecksum);
        case ChecksumAlgorithm::None:
            return true;
    }
    return false;
}

}  // namespace
Status SocketServiceImpl::Service::requestFileTransfer(
    ServerContext* context, const FileTransferRequest* request,
    FileTransferResponse* response) {
    LogWhoCalledMe(context, "requestFileTransfer");

    LOG(INFO) << "Received file transfer request for path: "
              << request->file_path()
              << (request->is_upload() ? " (upload)" : " (download)");

    // 1. Create UUID for the transfer. Transfers are looked up solely by this
    // id, so draw it straight from the OS CSPRNG rather than a single-seeded
    // mt19937 (whose stream a client could otherwise predict from observed
    // ids and use to hijack another in-flight transfer).
    std::random_device rd;
    std::array<std::uint8_t, 16> uuid_bytes{};
    for (auto& uuid_byte : uuid_bytes) {
        uuid_byte = static_cast<std::uint8_t>(rd());
    }
    uuids::uuid uuid(uuid_bytes.begin(), uuid_bytes.end());
    response->set_uuid(uuids::to_string(uuid));
    // 2. Create file stream and store in activeTransfers_
    TranferEntry entry;

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

        if (request->file_checksum_algorithm() != ChecksumAlgorithm::None) {
            if (!request->has_file_checksum() ||
                request->file_checksum().empty()) {
                response->set_accepted(false);
                response->set_reject_message(
                    "Checksum algorithm set without checksum data");
                return Status::OK;
            }
            entry.checksumAlgorithm = request->file_checksum_algorithm();
            entry.checksum = request->file_checksum();
        }
        entry.overwriteExisting = request->overwrite_existing();
        entry.destinationPath = request->file_path();

        // For upload, we create a temporary file to store incoming data
        entry.filePath = std::filesystem::temp_directory_path() /
                         fmt::format("upload_{}.tmp", uuids::to_string(uuid));

        // Reject absurd sizes before pre-allocating: file_size is
        // client-supplied and resize_file would otherwise let a single request
        // exhaust the disk.
        constexpr std::uintmax_t kMaxTransferSize = 2ULL << 30;  // 2 GiB
        if (request->file_size() > kMaxTransferSize) {
            response->set_accepted(false);
            response->set_reject_message(
                "Requested file size exceeds maximum allowed");
            LOG(ERROR) << "Rejecting upload exceeding size limit: "
                       << request->file_size() << " > " << kMaxTransferSize;
            return Status::OK;
        }
        entry.totalSize = request->file_size();
        // Create the file first, then resize
        entry.fileStream.open(entry.filePath, std::ios::binary | std::ios::out);
        entry.fileStream.close();
        std::filesystem::resize_file(entry.filePath, entry.totalSize, ec);
        if (ec) {
            response->set_accepted(false);
            response->set_reject_message("Failed to allocate file for upload");
            LOG(ERROR) << "Failed to resize upload file " << entry.filePath
                       << ": " << ec.message();
            std::filesystem::remove(entry.filePath, ec);
            return Status::OK;
        }

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
        auto [it, inserted] = activeTransfers_.try_emplace(
            uuids::to_string(uuid), std::move(entry));
        if (!inserted) {
            response->set_accepted(false);
            response->set_reject_message("UUID collision detected");
            LOG(ERROR) << "File transfer with UUID already exists: "
                       << uuids::to_string(uuid);
            return Status::OK;
        }
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
        std::vector<char> buffer(CHUNK_SIZE);
        std::streamsize bytesRead = 0;
        {
            std::lock_guard<std::mutex> lock(activeTransfersMutex_);
            auto it = activeTransfers_.find(msg.uuid());
            if (it == activeTransfers_.end()) {
                // Invalid UUID
                FileChunkResponse response;
                response.set_success(false);
                response.set_retry(false);
                LOG(ERROR) << "Invalid UUID for file download: "
                           << std::quoted(msg.uuid());
                stream->Write(response);
                return Status::OK;
            }

            it->second.fileStream.seekg(msg.chunk_idx() * CHUNK_SIZE,
                                        std::ios::beg);
            if (!it->second.fileStream.good()) {
                // Seek failed
                FileChunkResponse response;
                response.set_success(false);
                response.set_retry(true);
                LOG(ERROR) << "Seek failed during file download: "
                           << msg.uuid();
                stream->Write(response);
                return Status::OK;
            }

            it->second.fileStream.read(buffer.data(), buffer.size());
            bytesRead = it->second.fileStream.gcount();
        }

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
        {
            std::lock_guard<std::mutex> lock(activeTransfersMutex_);
            auto it = activeTransfers_.find(msg.uuid());
            if (it == activeTransfers_.end()) {
                // Invalid UUID
                FileChunkResponse response;
                response.set_success(false);
                response.set_retry(false);
                LOG(ERROR) << "Invalid UUID for file upload: "
                           << std::quoted(msg.uuid());
                stream->Write(response);
                return Status::OK;
            }
            if (msg.chunk_offset() < 0 ||
                static_cast<std::uintmax_t>(msg.chunk_offset()) +
                        msg.chunk_data().size() >
                    it->second.totalSize) {
                FileChunkResponse response;
                response.set_success(false);
                response.set_retry(false);
                LOG(ERROR) << "Invalid chunk range for upload UUID "
                           << std::quoted(msg.uuid());
                stream->Write(response);
                return Status::OK;
            }

            it->second.fileStream.seekp(msg.chunk_offset(), std::ios::beg);
            if (!it->second.fileStream.good()) {
                // Seek failed
                FileChunkResponse response;
                response.set_success(false);
                response.set_retry(true);
                LOG(ERROR) << "Seek failed during file upload: "
                           << msg.uuid();
                stream->Write(response);
                return Status::OK;
            }
            it->second.fileStream.write(msg.chunk_data().data(),
                                        msg.chunk_data().size());
            if (!it->second.fileStream.good()) {
                // Write failed
                FileChunkResponse response;
                response.set_success(false);
                response.set_retry(true);
                LOG(ERROR) << "Write failed during file upload: "
                           << msg.uuid();
                stream->Write(response);
                return Status::OK;
            }
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
        if (request->file_path() != entry.destinationPath) {
            response->set_code(GenericResponseCode::ErrorInvalidArgument);
            response->set_message(
                "Upload destination path mismatch for this transfer");
            LOG(ERROR) << "Upload destination path mismatch for UUID "
                       << request->uuid() << ": expected "
                       << entry.destinationPath.string() << ", got "
                       << request->file_path();
            std::filesystem::remove(entry.filePath);
            activeTransfers_.erase(it);
            return Status::OK;
        }
        if (entry.checksumAlgorithm != ChecksumAlgorithm::None &&
            !verify_file_checksum(entry.filePath, entry.checksumAlgorithm,
                                  entry.checksum)) {
            response->set_code(GenericResponseCode::ErrorInvalidArgument);
            response->set_message("Checksum mismatch");
            LOG(ERROR) << "Checksum mismatch for upload: " << request->uuid();
            std::filesystem::remove(entry.filePath);
            activeTransfers_.erase(it);
            return Status::OK;
        }
        // Enforce no-overwrite at finalize time as well: the existence check in
        // requestFileTransfer is a TOCTOU window, and std::filesystem::rename
        // silently replaces an existing destination on POSIX, so a plain rename
        // would clobber even when overwrite was not requested.
        {
            std::error_code existsEc;
            if (!entry.overwriteExisting &&
                std::filesystem::exists(entry.destinationPath, existsEc)) {
                response->set_code(GenericResponseCode::ErrorInvalidArgument);
                response->set_message("Destination already exists");
                LOG(ERROR) << "Destination already exists for upload: "
                           << entry.destinationPath.string();
                std::filesystem::remove(entry.filePath, existsEc);
                activeTransfers_.erase(it);
                return Status::OK;
            }
        }
        // Move temporary file to final destination. Prefer rename (atomic, no
        // sparse-hole materialization) but fall back to copy when the temp dir
        // and destination live on different filesystems (rename -> EXDEV).
        try {
            std::error_code moveEc;
            if (entry.overwriteExisting) {
                std::filesystem::remove(entry.destinationPath, moveEc);
            }
            std::filesystem::rename(entry.filePath, entry.destinationPath,
                                    moveEc);
            if (moveEc == std::errc::cross_device_link) {
                std::filesystem::copy_file(
                    entry.filePath, entry.destinationPath,
                    std::filesystem::copy_options::overwrite_existing);
                std::filesystem::remove(entry.filePath);
            } else if (moveEc) {
                throw std::filesystem::filesystem_error(
                    "Failed to move uploaded file", entry.filePath,
                    entry.destinationPath, moveEc);
            }
            LOG(INFO) << "File uploaded successfully to: "
                      << entry.destinationPath.string();
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
