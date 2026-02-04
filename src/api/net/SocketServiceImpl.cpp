#include "SocketServiceImpl.hpp"

#include <absl/log/log.h>
#include <fmt/format.h>
#include <grpcpp/grpcpp.h>
#include <grpcpp/server_builder.h>
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
#include "tgbot/TgException.h"

using grpc::ServerContext;
using grpc::Status;
using namespace tgbot::proto::socket;

class SocketServiceImpl::Service : public SocketService::Service {
   public:
    explicit Service(TgBotApi* api, SpamBlockBase* spamBlock)
        : api_(api), spamBlock_(spamBlock) {}

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

   private:
    TgBotApi* api_;
    SpamBlockBase* spamBlock_;

    struct TranferEntry {
        std::fstream fileStream;
        std::filesystem::path filePath;
        std::uintmax_t totalSize = 0;
        int chunk_count;
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

    if (activeTransfers_.contains(uuids::to_string(uuid))) {
        response->set_accepted(false);
        LOG(ERROR) << "File transfer with UUID already exists: "
                   << uuids::to_string(uuid);
        return Status::OK;
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

        // For upload, we create a temporary file to store incoming data
        entry.filePath = std::filesystem::temp_directory_path() /
                         fmt::format("upload_{}.tmp", uuids::to_string(uuid));

        entry.totalSize = request->file_size();
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
    activeTransfers_.emplace(uuids::to_string(uuid), std::move(entry));
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

        fileStream.seekg(msg.chunk_offset(), std::ios::beg);
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
                                     Url* url)
    : api_(api), spamBlock_(spamBlock), url_(url) {
    service_ = std::make_unique<Service>(api, spamBlock);
    impl_ = std::make_unique<Impl>();
}

SocketServiceImpl::~SocketServiceImpl() = default;