// #undef CPPHTTPLIB_OPENSSL_SUPPORT
// #undef CPPHTTPLIB_ZLIB_SUPPORT
// #undef CPPHTTPLIB_BROTLI_SUPPORT

#include <absl/log/log.h>
#include <absl/strings/ascii.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <TgBotWebpage.hpp>
#include <cstdint>
#include <expected_cpp20>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <nlohmann/json.hpp>
#include <utility>

#include "Socket_service.pb.h"
#include "SystemMonitor_service.grpc.pb.h"
#include "SystemMonitor_service.pb.h"

constexpr bool WEBSERVER_INBOUND_VERBOSE = false;

namespace {
std::string getOrEmpty(const std::string& in) {
    return in.empty() ? in : "Empty";
}
}  // namespace

void TgBotWebServerBase::loggerFn(const httplib::Request& req,
                                  const httplib::Response& res) {
    static std::mutex logger_lock;
    std::lock_guard<std::mutex> lock(logger_lock);
    DLOG(INFO) << "=============== Inbound HTTP Request ====================";
    DLOG(INFO) << "HTTP request method: " << req.method;
    DLOG(INFO) << "Remote Address: " << req.remote_addr;
    if constexpr (WEBSERVER_INBOUND_VERBOSE) {
        DLOG(INFO) << "Remote Port: " << req.remote_port;
        DLOG(INFO) << "Host: " << req.get_header_value("Host");
        DLOG(INFO) << "User-Agent: " << req.get_header_value("User-Agent");
        DLOG(INFO) << "Referer: "
                   << getOrEmpty(req.get_header_value("Referer"));
        DLOG(INFO) << "Accept: " << req.get_header_value("Accept");
    }
    DLOG(INFO) << "Requested filepath: " << std::quoted(req.path);
    DLOG(INFO) << "Status: " << res.status;
    DLOG(INFO) << "=========================================================";
}

void TgBotWebServerBase::startServer() {
    auto ret = svr.set_mount_point(Constants::kWebRootNode,
                                   webServerRootPath.string());
    if (!ret) {
        LOG(ERROR) << "Failed to switch mount point to " << webServerRootPath;
        return;
    } else {
        LOG(INFO) << "Web page mount point: " << webServerRootPath.string();
    }

    // Static pages
    svr.Get(Constants::kWebRootNode,
            [](const httplib::Request&, httplib::Response& res) {
                res.set_redirect(Constants::kAboutPage);
            });
    svr.Get(Constants::kAboutPage,
            [this](const httplib::Request& req, httplib::Response& res) {
                callback.showIndex(req, res);
            });

    // --- REST API V1 ---

    // 1. Messages (Noun)
    // POST /api/v1/messages
    svr.Post(Constants::kAPIV1Messages,
             [this](const httplib::Request& req, httplib::Response& res) {
                 callback.handleMessageCreate(req, res);
             });

    // 2. Stats
    // GET /api/v1/stats
    svr.Get(Constants::kAPIV1Stats,
            [this](const httplib::Request& req, httplib::Response& res) {
                callback.handleStats(req, res);
            });

    // 3. Votes
    // POST /api/v1/votes
    svr.Post(Constants::kAPIVotesNode,
             [this](const httplib::Request& req, httplib::Response& res) {
                 callback.handleAPIVotes(req, res);
             });

    // 4. Chats
    // PUT /api/v1/chats/:chat_id (Updates/Sets alias)
    std::string chatPath = Constants::kAPIV1ChatsNode;
    svr.Put(chatPath + "/:chat_id",
            [this](const httplib::Request& req, httplib::Response& res) {
                callback.handleChatPut(req, res);
            });
    // DELETE /api/v1/chats/:chat_id (Removes alias)
    svr.Delete(chatPath + "/:chat_id",
               [this](const httplib::Request& req, httplib::Response& res) {
                   callback.handleChatDelete(req, res);
               });
    // GET /api/v1/chats?chat_name=... (Search)
    svr.Get(Constants::kAPIV1ChatsNode,
            [this](const httplib::Request& req, httplib::Response& res) {
                callback.handleChatsGet(req, res);
            });

    // 5. Media
    // PUT /api/v1/media/:media_id (Updates/Sets alias)
    std::string mediaPath = Constants::kAPIV1MediaNode;
    svr.Put(mediaPath + "/:media_id",
            [this](const httplib::Request& req, httplib::Response& res) {
                callback.handleMediaPut(req, res);
            });
    // DELETE /api/v1/media/:media_id (Removes alias)
    svr.Delete(mediaPath + "/:media_id",
               [this](const httplib::Request& req, httplib::Response& res) {
                   callback.handleMediaDelete(req, res);
               });
    // GET /api/v1/media?alias=... (Search)
    svr.Get(Constants::kAPIV1MediaNode,
            [this](const httplib::Request& req, httplib::Response& res) {
                callback.handleMediaGet(req, res);
            });

    // 6. Hardware
    svr.Get(Constants::kAPIV1Hardware,
            [this](const httplib::Request& req, httplib::Response& res) {
                callback.handleHardware(req, res);
            });

    svr.set_logger(TgBotWebServerBase::loggerFn);
    svr.listen(Constants::kBindToIp, port);
}

void TgBotWebServerBase::stopServer() { svr.stop(); }

TgBotWebServerBase::TgBotWebServerBase(int serverPort,
                                       std::filesystem::path serverPath)
    : port(serverPort),
      callback(this),
      webServerRootPath(std::move(serverPath)) {}

void TgBotWebServerBase::Callbacks::showIndex(const httplib::Request& req,
                                              httplib::Response& res) {
    std::ifstream stream(server->webServerRootPath / "about.html");
    if (!stream.is_open()) {
        LOG(ERROR) << "Failed to open web page index";
        res.status = httplib::StatusCode::InternalServerError_500;
        res.set_content("500 Internal Server Error", "text/plain");
    } else {
        std::string str((std::istreambuf_iterator<char>(stream)),
                        std::istreambuf_iterator<char>());

        res.status = httplib::StatusCode::OK_200;
        res.set_content(str, "text/html");
    }
}

#include "Socket_service.grpc.pb.h"

using tgbot::builder::system_monitor::SystemMonitorService;
using tgbot::proto::socket::SocketService;

struct TgBotWebServerBase::Callbacks::Connection {
    struct Stubs {
        std::unique_ptr<SocketService::Stub> sock;
        std::unique_ptr<SystemMonitorService::Stub> mon;
    } http, https;

    std::string successFalse;
};

namespace {

void logIfHasHeader(const httplib::Request& req, const char* headerName) {
    if (req.has_header(headerName)) {
        LOG(INFO) << headerName << ": " << req.get_header_value(headerName);
    }
}

compat::expected<nlohmann::json, int> acceptAPIRequest(
    const httplib::Request& req) {
    constexpr const char* kContentType = "Content-Type";
    constexpr const char* kContentTypeJson = "application/json";

    if (!(req.has_header(kContentType) &&
          req.get_header_value(kContentType) == kContentTypeJson)) {
        LOG(ERROR) << "Invalid API request: Missing or invalid Content-Type";
        return compat::unexpected(httplib::StatusCode::BadRequest_400);
    }

    logIfHasHeader(req, TgBotWebServerBase::Constants::kHeaderRealIp);
    logIfHasHeader(req, TgBotWebServerBase::Constants::kHeaderForwardedFor);
    logIfHasHeader(req, TgBotWebServerBase::Constants::kHeaderClientVerify);
    logIfHasHeader(req, TgBotWebServerBase::Constants::kHeaderClientDn);
    logIfHasHeader(req,
                   TgBotWebServerBase::Constants::kHeaderClientFingerprint);

    nlohmann::json document{};
    try {
        document = nlohmann::json::parse(req.body);
    } catch (const nlohmann::json::parse_error& e) {
        LOG(ERROR) << "Failed to parse JSON: " << e.what();
        return compat::unexpected(httplib::StatusCode::BadRequest_400);
    }

    return document;
}

using tgbot::proto::socket::FileType;

std::optional<FileType> fileTypeFromString(const std::string& type) {
    auto new_type = absl::AsciiStrToLower(type);
    if (new_type == "photo") {
        return FileType::PHOTO;
    } else if (new_type == "video") {
        return FileType::VIDEO;
    } else if (new_type == "audio") {
        return FileType::AUDIO;
    } else if (new_type == "document") {
        return FileType::DOCUMENT;
    } else if (new_type == "sticker") {
        return FileType::STICKER;
    } else if (new_type == "gif") {
        return FileType::GIF;
    } else if (new_type == "dice") {
        return FileType::DICE;
    } else {
        return std::nullopt;
    }
}
}  // namespace

using tgbot::proto::socket::BotInfo;
using tgbot::proto::socket::GenericResponse;
using tgbot::proto::socket::SendMessageRequest;
using tgbot::proto::socket::SocketService;

// Local gRPC connection ports (behind Nginx or other reverse proxy)
constexpr int kLocalHttpsConnection = 443;
constexpr int kLocalHttpConnection = 80;

TgBotWebServerBase::Callbacks::Callbacks(TgBotWebServerBase* server)
    : server(server), _conn(std::make_unique<Connection>()) {
    // Initialize gRPC connection
    // We can lookup two addresses: one for HTTP, one for HTTPS

    auto httpChannel =
        grpc::CreateChannel("127.0.0.1:80", grpc::InsecureChannelCredentials());
    auto httpsChannel = grpc::CreateChannel("127.0.0.1:443",
                                            grpc::InsecureChannelCredentials());

    _conn->http.sock = SocketService::NewStub(httpChannel);
    _conn->https.sock = SocketService::NewStub(httpsChannel);
    _conn->http.mon = SystemMonitorService::NewStub(httpChannel);
    _conn->https.mon = SystemMonitorService::NewStub(httpsChannel);
    _conn->successFalse = R"({"success": false})";
}

TgBotWebServerBase::Callbacks::~Callbacks() = default;

void TgBotWebServerBase::Callbacks::handleMessageCreate(
    const httplib::Request& req, httplib::Response& res) {
    auto maybeDocument = acceptAPIRequest(req);
    if (!maybeDocument.has_value()) {
        res.status = maybeDocument.error();
        return;
    }

    grpc::ClientContext context;
    SendMessageRequest grpcRequest;
    GenericResponse grpcResponse;

    const auto& document = maybeDocument.value();

    // Chat ID
    if (document.contains(Constants::kAPIKeyChatId) &&
        document[Constants::kAPIKeyChatId].is_number_integer()) {
        grpcRequest.set_chat_id(
            document[Constants::kAPIKeyChatId].get<int64_t>());
    } else {
        LOG(ERROR) << "Invalid API request: Missing chat_id value";
        res.status = httplib::StatusCode::BadRequest_400;
        return;
    }

    // Text
    if (document.contains(Constants::kAPIKeyText) &&
        document[Constants::kAPIKeyText].is_string()) {
        grpcRequest.set_text(
            document[Constants::kAPIKeyText].get<std::string>());
    } else {
        LOG(ERROR) << "Invalid API request: Missing text value";
        res.status = httplib::StatusCode::BadRequest_400;
        return;
    }

    // Optional: FileType
    if (document.contains(Constants::kAPIKeyFileType) &&
        document[Constants::kAPIKeyFileType].is_string()) {
        auto type = document[Constants::kAPIKeyFileType].get<std::string>();
        auto fileTypeOpt = fileTypeFromString(type);
        if (fileTypeOpt.has_value()) {
            grpcRequest.set_file_type(fileTypeOpt.value());
        } else {
            LOG(ERROR) << "Invalid API request: Unknown file_type value";
            res.status = httplib::StatusCode::BadRequest_400;
            return;
        }
    }

    // Optional: File Data
    if (document.contains(Constants::kAPIKeyFileData) &&
        document[Constants::kAPIKeyFileData].is_string()) {
        // Unimplemented: Base64 decode
        LOG(ERROR) << "File data upload is unimplemented";
        res.status = httplib::StatusCode::NotImplemented_501;
        return;
    }

    // Optional: File Path, this is again, unimplemented
    if (document.contains(Constants::kAPIKeyFilePath) &&
        document[Constants::kAPIKeyFilePath].is_string()) {
        LOG(ERROR) << "File path upload is unimplemented";
        res.status = httplib::StatusCode::NotImplemented_501;
        return;
    }

    // Optional: File ID
    if (document.contains(Constants::kAPIKeyFileId) &&
        document[Constants::kAPIKeyFileId].is_string()) {
        grpcRequest.set_file_id(
            document[Constants::kAPIKeyFileId].get<std::string>());
    }

    LOG(INFO) << "API Req sendMessage to chat_id: " << grpcRequest.chat_id()
              << " with text: " << grpcRequest.text();

    grpc::Status status =
        _conn->https.sock->sendMessage(&context, grpcRequest, &grpcResponse);
    if (status.ok()) {
        res.status = httplib::StatusCode::OK_200;
    } else {
        LOG(ERROR) << "gRPC SendMessage failed: " << status.error_message();
        res.status = httplib::StatusCode::InternalServerError_500;
    }

    nlohmann::json responseJson;
    responseJson["success"] = status.ok();
    responseJson["code"] = grpcResponse.code();
    responseJson["message"] = grpcResponse.message();
    res.set_content(responseJson.dump(), "application/json");
}

void TgBotWebServerBase::Callbacks::handleStats(const httplib::Request& req,
                                                httplib::Response& res) {
    BotInfo grpcResponse;
    grpc::ClientContext context;
    grpc::Status status = _conn->https.sock->info(
        &context, google::protobuf::Empty{}, &grpcResponse);
    if (status.ok()) {
        res.status = httplib::StatusCode::OK_200;
    } else {
        LOG(ERROR) << "gRPC Info failed: " << status.error_message();
        res.status = httplib::StatusCode::InternalServerError_500;
        res.set_content(_conn->successFalse, "application/json");
        return;
    }
    nlohmann::json responseJson;
    responseJson["success"] = status.ok();
    responseJson["username"] = grpcResponse.username();
    responseJson["user_id"] = grpcResponse.user_id();
    responseJson["operating_system"] = grpcResponse.operating_system();
    responseJson["uptime"]["days"] = grpcResponse.uptime().days();
    responseJson["uptime"]["hours"] = grpcResponse.uptime().hours();
    responseJson["uptime"]["minutes"] = grpcResponse.uptime().minutes();
    responseJson["uptime"]["seconds"] = grpcResponse.uptime().seconds();
    res.set_content(responseJson.dump(), "application/json");
}

void TgBotWebServerBase::Callbacks::handleChatsGet(const httplib::Request& req,
                                                   httplib::Response& res) {
    // 1. If query has chat_name
    if (!req.has_param(Constants::kAPIKeyChatName)) {
        LOG(ERROR) << "Invalid API request: Missing chat_name parameter";
        res.status = httplib::StatusCode::BadRequest_400;
        res.set_content(_conn->successFalse, "application/json");
        return;
    }
    std::string chatName = req.get_param_value(Constants::kAPIKeyChatName);
    // 2. Make gRPC call to get chat_id
    grpc::ClientContext context;
    tgbot::proto::socket::ChatAliasRequest grpcRequest;
    tgbot::proto::socket::ChatAliasResponse grpcResponse;
    grpcRequest.set_alias(chatName);
    grpc::Status status =
        _conn->https.sock->getChatAlias(&context, grpcRequest, &grpcResponse);
    if (!status.ok()) {
        LOG(ERROR) << "gRPC getChatAlias failed: " << status.error_message();
        res.status = httplib::StatusCode::InternalServerError_500;
        res.set_content(_conn->successFalse, "application/json");
        return;
    }
    // 3. Return JSON response with success and chat_id
    nlohmann::json responseJson;
    responseJson["success"] = status.ok() && grpcResponse.exists();
    responseJson["chat_id"] = grpcResponse.chat_id();
    res.status = httplib::StatusCode::OK_200;
    res.set_content(responseJson.dump(), "application/json");
}

void TgBotWebServerBase::Callbacks::handleChatPut(const httplib::Request& req,
                                                  httplib::Response& res) {
    // 1. Get ID from URL Path
    if (!req.has_param("chat_id")) {
        // Should be impossible if routed correctly
        res.status = httplib::StatusCode::BadRequest_400;
        return;
    }

    int64_t chatId = 0;
    try {
        chatId = std::stoll(req.path_params.at("chat_id"));
    } catch (...) {
        res.status = httplib::StatusCode::BadRequest_400;
        return;
    }

    // 2. Parse Body for Name
    auto maybeDocument = acceptAPIRequest(req);
    if (!maybeDocument.has_value()) {
        res.status = maybeDocument.error();
        return;
    }
    const auto& document = maybeDocument.value();

    if (!document.contains(Constants::kAPIKeyChatName) ||
        !document[Constants::kAPIKeyChatName].is_string()) {
        LOG(ERROR) << "Invalid API request: Missing chat_name in body";
        res.status = httplib::StatusCode::BadRequest_400;
        return;
    }
    std::string chatName =
        document[Constants::kAPIKeyChatName].get<std::string>();

    // 3. Execute gRPC Update
    grpc::ClientContext context;
    GenericResponse grpcResponse;
    tgbot::proto::socket::ChatAlias grpcRequest;
    grpcRequest.set_chat_id(chatId);
    grpcRequest.set_alias(chatName);

    grpc::Status status =
        _conn->https.sock->setChatAlias(&context, grpcRequest, &grpcResponse);

    if (!status.ok()) {
        LOG(ERROR) << "gRPC setChatAlias failed: " << status.error_message();
        res.status = httplib::StatusCode::InternalServerError_500;
        res.set_content(_conn->successFalse, "application/json");
        return;
    }

    res.status = httplib::StatusCode::OK_200;
    nlohmann::json responseJson;
    responseJson["success"] = true;
    res.set_content(responseJson.dump(), "application/json");
}

void TgBotWebServerBase::Callbacks::handleChatDelete(
    const httplib::Request& req, httplib::Response& res) {
    // 1. Get ID from URL Path
    int64_t chatId = 0;
    try {
        chatId = std::stoll(req.path_params.at("chat_id"));
    } catch (...) {
        res.status = httplib::StatusCode::BadRequest_400;
        return;
    }

    // 2. Execute gRPC Delete
    grpc::ClientContext context;
    GenericResponse grpcResponse;
    tgbot::proto::socket::ChatAliasRequest grpcRequest;
    grpcRequest.set_chat_id(chatId);

    grpc::Status status = _conn->https.sock->deleteChatAlias(
        &context, grpcRequest, &grpcResponse);

    if (!status.ok()) {
        LOG(ERROR) << "gRPC deleteChatAlias failed: " << status.error_message();
        res.status = httplib::StatusCode::InternalServerError_500;
        res.set_content(_conn->successFalse, "application/json");
        return;
    }

    // 204 No Content is standard for DELETE, but 200 with JSON is also fine.
    res.status = httplib::StatusCode::OK_200;
    nlohmann::json responseJson;
    responseJson["success"] = true;
    res.set_content(responseJson.dump(), "application/json");
}

void TgBotWebServerBase::Callbacks::handleMediaGet(const httplib::Request& req,
                                                   httplib::Response& res) {
    // 1. If query has alias
    if (!req.has_param(Constants::kAPIKeyAlias)) {
        LOG(ERROR) << "Invalid API request: Missing alias parameter";
        res.status = httplib::StatusCode::BadRequest_400;
        res.set_content(_conn->successFalse, "application/json");
        return;
    }
    std::string alias = req.get_param_value(Constants::kAPIKeyAlias);
    // 2. Make gRPC call to get media_id
    grpc::ClientContext context;
    tgbot::proto::socket::MediaAliasRequest grpcRequest;
    tgbot::proto::socket::MediaAliasResponse grpcResponse;
    grpcRequest.set_alias(alias);
    grpc::Status status =
        _conn->https.sock->getMediaAlias(&context, grpcRequest, &grpcResponse);
    if (!status.ok()) {
        LOG(ERROR) << "gRPC getMediaAlias failed: " << status.error_message();
        res.status = httplib::StatusCode::InternalServerError_500;
        res.set_content(_conn->successFalse, "application/json");
        return;
    }
    // 3. Return JSON response with success and media_id
    nlohmann::json responseJson;
    responseJson["success"] = status.ok() && grpcResponse.exists();
    std::vector<std::string> mediaIds(grpcResponse.media_id().begin(),
                                      grpcResponse.media_id().end());
    responseJson["media_id"] = mediaIds;
    res.status = httplib::StatusCode::OK_200;
    res.set_content(responseJson.dump(), "application/json");
}

void TgBotWebServerBase::Callbacks::handleMediaPut(const httplib::Request& req,
                                                   httplib::Response& res) {
    // 1. Get ID from URL
    std::string mediaId = req.path_params.at("media_id");

    // 2. Parse Body for Alias List and Type
    auto maybeDocument = acceptAPIRequest(req);
    if (!maybeDocument.has_value()) {
        res.status = maybeDocument.error();
        return;
    }
    const auto& document = maybeDocument.value();

    // Check Alias Array
    if (!document.contains(Constants::kAPIKeyAlias) ||
        !document[Constants::kAPIKeyAlias].is_array()) {
        res.status = httplib::StatusCode::BadRequest_400;
        return;
    }
    auto aliases =
        document[Constants::kAPIKeyAlias].get<std::vector<std::string>>();

    // Check Media Type
    std::optional<FileType> mediaType;
    if (document.contains(Constants::kAPIKeyMediaType) &&
        document[Constants::kAPIKeyMediaType].is_string()) {
        auto typeStr = document[Constants::kAPIKeyMediaType].get<std::string>();
        mediaType = fileTypeFromString(typeStr);
    }

    if (!mediaType.has_value()) {
        res.status = httplib::StatusCode::BadRequest_400;
        return;
    }

    // 3. Execute gRPC
    grpc::ClientContext context;
    GenericResponse grpcResponse;
    tgbot::proto::socket::MediaAlias grpcRequest;
    grpcRequest.set_media_id(mediaId);
    grpcRequest.set_media_type(*mediaType);
    for (const auto& a : aliases) {
        grpcRequest.add_alias(a);
    }

    grpc::Status status =
        _conn->https.sock->setMediaAlias(&context, grpcRequest, &grpcResponse);

    if (!status.ok()) {
        res.status = httplib::StatusCode::InternalServerError_500;
        res.set_content(_conn->successFalse, "application/json");
        return;
    }

    res.status = httplib::StatusCode::OK_200;
    nlohmann::json responseJson;
    responseJson["success"] = true;
    res.set_content(responseJson.dump(), "application/json");
}

void TgBotWebServerBase::Callbacks::handleMediaDelete(
    const httplib::Request& req, httplib::Response& res) {
    // 1. Get ID from URL
    std::string mediaId = req.path_params.at("media_id");

    // 2. Execute gRPC
    grpc::ClientContext context;
    GenericResponse grpcResponse;
    tgbot::proto::socket::MediaAliasRequest grpcRequest;
    grpcRequest.set_media_id(mediaId);

    grpc::Status status = _conn->https.sock->deleteMediaAlias(
        &context, grpcRequest, &grpcResponse);

    if (!status.ok()) {
        res.status = httplib::StatusCode::InternalServerError_500;
        res.set_content(_conn->successFalse, "application/json");
        return;
    }

    res.status = httplib::StatusCode::OK_200;
    nlohmann::json responseJson;
    responseJson["success"] = true;
    res.set_content(responseJson.dump(), "application/json");
}

void TgBotWebServerBase::Callbacks::handleHardware(const httplib::Request& req,
                                                   httplib::Response& res) {
    // Try to connect to gRPC server
    tgbot::builder::system_monitor::SystemInfo grpcResponse;
    tgbot::builder::system_monitor::GetSystemInfoRequest grpcRequest;
#ifdef _WIN32
    grpcRequest.set_disk_path("C:\\");
#else
    grpcRequest.set_disk_path("/");
#endif
    grpc::ClientContext context;
    grpc::Status status =
        _conn->https.mon->GetSystemInfo(&context, grpcRequest, &grpcResponse);
    if (!status.ok()) {
        LOG(ERROR) << "gRPC GetSystemInfo failed: " << status.error_message();
        res.status = httplib::StatusCode::InternalServerError_500;
        res.set_content(_conn->successFalse, "application/json");
        return;
    }

    grpc::ClientContext context2;
    tgbot::builder::system_monitor::SystemStats stats;
    status = _conn->https.mon->GetStats(&context2, google::protobuf::Empty{},
                                        &stats);
    if (!status.ok()) {
        LOG(ERROR) << "gRPC GetStats failed: " << status.error_message();
        res.status = httplib::StatusCode::InternalServerError_500;
        res.set_content(_conn->successFalse, "application/json");
        return;
    }
    nlohmann::json responseJson;
    responseJson["success"] = true;

    // CPU
    responseJson[Constants::kAPIKeyCPU]["usage_percent"] =
        stats.cpu_usage_percent();
    responseJson[Constants::kAPIKeyCPU]["core_count"] =
        grpcResponse.cpu_cores();
    responseJson[Constants::kAPIKeyCPU]["name"] = grpcResponse.cpu_name();

    // Memory
    responseJson[Constants::kAPIKeyMemory]["total_mbytes"] =
        grpcResponse.memory_total_mb();
    responseJson[Constants::kAPIKeyMemory]["used_mbytes"] =
        stats.memory_used_mb();

    // Disk
    responseJson[Constants::kAPIKeyDisk]["total_gbytes"] =
        grpcResponse.disk_total_gb();
    responseJson[Constants::kAPIKeyDisk]["used_gbytes"] =
        grpcResponse.disk_used_gb();

    // OS
    responseJson[Constants::kAPIKeyOS]["name"] = grpcResponse.os_name();
    responseJson[Constants::kAPIKeyOS]["version"] = grpcResponse.os_version();
    responseJson[Constants::kAPIKeyOS]["kernel_version"] =
        grpcResponse.kernel_version();
    responseJson[Constants::kAPIKeyOS]["hostname"] = grpcResponse.hostname();
    responseJson[Constants::kAPIKeyOS]["uptime_seconds"] =
        stats.uptime_seconds();
    res.status = httplib::StatusCode::OK_200;
    res.set_content(responseJson.dump(), "application/json");
}

void TgBotWebServerBase::Callbacks::handleAPIVotes(const httplib::Request& req,
                                                   httplib::Response& res) {
    std::string maybeVote;
    auto maybeDocument = acceptAPIRequest(req);
    if (!maybeDocument.has_value()) {
        res.status = maybeDocument.error();
        return;
    }

    const auto& document = maybeDocument.value();
    if (document.contains(Constants::kAPIVotesKey) &&
        document[Constants::kAPIVotesKey].is_string()) {
        maybeVote = document[Constants::kAPIVotesKey].get<std::string>();
    }
    if (maybeVote.empty()) {
        LOG(ERROR) << "Invalid API request: Missing vote value";
        res.status = httplib::StatusCode::BadRequest_400;
        return;
    }
    LOG(INFO) << "API Req vote: " << maybeVote;
    res.status = httplib::StatusCode::OK_200;
    nlohmann::json responseJson;
    responseJson["success"] = true;
    res.set_content(responseJson.dump(), "application/json");
}