// #undef CPPHTTPLIB_OPENSSL_SUPPORT
// #undef CPPHTTPLIB_ZLIB_SUPPORT
// #undef CPPHTTPLIB_BROTLI_SUPPORT

#include <absl/log/log.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>

#include <TgBotWebpage.hpp>
#include <cstdint>
#include <expected_cpp20>
#include <fstream>
#include <iomanip>
#include <memory>
#include <nlohmann/json.hpp>
#include <utility>

#include "Socket_service.pb.h"

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
        LOG(INFO) << "Web page mount point: " << webServerRootPath.string()
                  << " as root directory";
    }
    svr.Get(Constants::kWebRootNode,
            [](const httplib::Request& req, httplib::Response& res) {
                res.set_redirect(Constants::kAboutPage);
            });
    svr.Get(Constants::kAboutPage,
            [this](const httplib::Request& req, httplib::Response& res) {
                callback.showIndex(req, res);
            });
    svr.Post(Constants::kAPIVotesNode,
             [this](const httplib::Request& req, httplib::Response& res) {
                 callback.handleAPIVotes(req, res);
             });
    svr.Post(Constants::kAPIV1SendMessage,
             [this](const httplib::Request& req, httplib::Response& res) {
                 callback.handleSendMessage(req, res);
             });
    svr.Get(Constants::kAPIV1Stats,
            [this](const httplib::Request& req, httplib::Response& res) {
                callback.handleStats(req, res);
            });
    svr.Post(Constants::kAPIV1ChatsNode,
             [this](const httplib::Request& req, httplib::Response& res) {
                 callback.handleChats(req, res);
             });
    svr.Get(Constants::kAPIV1ChatsNode,
            [this](const httplib::Request& req, httplib::Response& res) {
                callback.handleChatsGet(req, res);
            });
    svr.Post(Constants::kAPIV1MediaNode,
             [this](const httplib::Request& req, httplib::Response& res) {
                 callback.handleMedia(req, res);
             });
    svr.Get(Constants::kAPIV1MediaNode,
            [this](const httplib::Request& req, httplib::Response& res) {
                callback.handleMediaGet(req, res);
            });
    svr.set_logger(TgBotWebServerBase::loggerFn);
    svr.listen(Constants::kBindToIp, port);
}

void TgBotWebServerBase::stopServer() { svr.stop(); }

TgBotWebServerBase::TgBotWebServerBase(int serverPort,
                                       std::filesystem::path serverPath,
                                       std::string grpcServerAddr)
    : _grpcServerAddr(std::move(grpcServerAddr)),
      port(serverPort),
      callback(this),
      webServerRootPath(std::move(serverPath)) {
    LOG(INFO) << "TgBotWebServerBase initialized with gRPC server at "
              << _grpcServerAddr;
}

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

struct TgBotWebServerBase::Callbacks::Connection {
    std::unique_ptr<tgbot::proto::socket::SocketService::Stub> stub;
    std::shared_ptr<grpc::Channel> channel;
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
}  // namespace

using tgbot::proto::socket::BotInfo;
using tgbot::proto::socket::FileType;
using tgbot::proto::socket::GenericResponse;
using tgbot::proto::socket::SendMessageRequest;
using tgbot::proto::socket::SocketService;

TgBotWebServerBase::Callbacks::Callbacks(TgBotWebServerBase* server)
    : server(server), _conn(std::make_unique<Connection>()) {
    _conn->channel = grpc::CreateChannel(server->_grpcServerAddr,
                                         grpc::InsecureChannelCredentials());
    _conn->stub = SocketService::NewStub(_conn->channel);
    _conn->successFalse = R"({"success": false})";
    LOG(INFO) << "gRPC client connected to " << server->_grpcServerAddr;
}

TgBotWebServerBase::Callbacks::~Callbacks() = default;

void TgBotWebServerBase::Callbacks::handleSendMessage(
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
        if (type == "photo") {
            grpcRequest.set_file_type(FileType::PHOTO);
        } else if (type == "video") {
            grpcRequest.set_file_type(FileType::VIDEO);
        } else if (type == "audio") {
            grpcRequest.set_file_type(FileType::AUDIO);
        } else if (type == "document") {
            grpcRequest.set_file_type(FileType::DOCUMENT);
        } else if (type == "sticker") {
            grpcRequest.set_file_type(FileType::STICKER);
        } else if (type == "gif") {
            grpcRequest.set_file_type(FileType::GIF);
        } else if (type == "dice") {
            grpcRequest.set_file_type(FileType::DICE);
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
        _conn->stub->sendMessage(&context, grpcRequest, &grpcResponse);
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
    grpc::Status status =
        _conn->stub->info(&context, google::protobuf::Empty{}, &grpcResponse);
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
        _conn->stub->getChatAlias(&context, grpcRequest, &grpcResponse);
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
        _conn->stub->getMediaAlias(&context, grpcRequest, &grpcResponse);
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

void TgBotWebServerBase::Callbacks::handleChats(const httplib::Request& req,
                                                httplib::Response& res) {
    std::optional<int64_t> chatId = std::nullopt;
    std::optional<std::string> chatName;
    auto maybeDocument = acceptAPIRequest(req);
    if (!maybeDocument.has_value()) {
        res.status = maybeDocument.error();
        return;
    }

    const auto& document = maybeDocument.value();
    if (document.contains(Constants::kAPIKeyChatId) &&
        document[Constants::kAPIKeyChatId].is_number_integer()) {
        chatId = document[Constants::kAPIKeyChatId].get<int64_t>();
    }
    if (!chatId.has_value()) {
        LOG(ERROR) << "Invalid API request: Missing chat_id value";
        res.status = httplib::StatusCode::BadRequest_400;
        return;
    }
    if (document.contains(Constants::kAPIKeyChatName) &&
        document[Constants::kAPIKeyChatName].is_string()) {
        chatName = document[Constants::kAPIKeyChatName].get<std::string>();
    }
    LOG(INFO) << "API Req chat_id: " << chatId.value();
    LOG(INFO) << "API Req chat_name: "
              << (chatName.has_value() ? chatName.value() : "null");

    grpc::ClientContext context;
    GenericResponse grpcResponse;
    if (chatName.has_value()) {
        tgbot::proto::socket::ChatAlias grpcRequest;
        grpcRequest.set_chat_id(chatId.value());
        grpcRequest.set_alias(chatName.value());
        grpc::Status status =
            _conn->stub->setChatAlias(&context, grpcRequest, &grpcResponse);
        if (!status.ok()) {
            LOG(ERROR) << "gRPC setChatAlias failed: "
                       << status.error_message();
            res.status = httplib::StatusCode::InternalServerError_500;
            res.set_content(_conn->successFalse, "application/json");
            return;
        }
    } else {
        tgbot::proto::socket::ChatAliasRequest grpcRequest;
        grpcRequest.set_chat_id(chatId.value());
        grpc::Status status =
            _conn->stub->deleteChatAlias(&context, grpcRequest, &grpcResponse);
        if (!status.ok()) {
            LOG(ERROR) << "gRPC deleteChatAlias failed: "
                       << status.error_message();
            res.status = httplib::StatusCode::InternalServerError_500;
            res.set_content(_conn->successFalse, "application/json");
            return;
        }
    }

    res.status = httplib::StatusCode::OK_200;
    nlohmann::json responseJson;
    responseJson["success"] = true;
    res.set_content(responseJson.dump(), "application/json");
}

void TgBotWebServerBase::Callbacks::handleMedia(const httplib::Request& req,
                                                httplib::Response& res) {
    std::optional<std::string> mediaId = std::nullopt;
    std::optional<std::vector<std::string>> alias;
    auto maybeDocument = acceptAPIRequest(req);
    if (!maybeDocument.has_value()) {
        res.status = maybeDocument.error();
        return;
    }

    const auto& document = maybeDocument.value();
    if (document.contains(Constants::kAPIKeyMediaId) &&
        document[Constants::kAPIKeyMediaId].is_string()) {
        mediaId = document[Constants::kAPIKeyMediaId].get<std::string>();
    }
    if (!mediaId.has_value()) {
        LOG(ERROR) << "Invalid API request: Missing media_id value";
        res.status = httplib::StatusCode::BadRequest_400;
        return;
    }
    if (document.contains(Constants::kAPIKeyAlias) &&
        document[Constants::kAPIKeyAlias].is_array()) {
        alias =
            document[Constants::kAPIKeyAlias].get<std::vector<std::string>>();
    }
    LOG(INFO) << "API Req media_id: " << mediaId.value();
    if (alias.has_value()) {
        LOG(INFO) << "API Req alias count: " << alias->size();
    } else {
        LOG(INFO) << "API Req alias: null";
    }

    grpc::ClientContext context;
    GenericResponse grpcResponse;
    if (alias.has_value()) {
        tgbot::proto::socket::MediaAlias grpcRequest;
        grpcRequest.set_media_id(mediaId.value());
        for (const auto& a : alias.value()) {
            grpcRequest.add_alias(a);
        }
        grpc::Status status =
            _conn->stub->setMediaAlias(&context, grpcRequest, &grpcResponse);
        if (!status.ok()) {
            LOG(ERROR) << "gRPC setMediaAlias failed: "
                       << status.error_message();
            res.status = httplib::StatusCode::InternalServerError_500;
            res.set_content(_conn->successFalse, "application/json");
            return;
        }
    } else {
        tgbot::proto::socket::MediaAliasRequest grpcRequest;
        grpcRequest.set_media_id(mediaId.value());
        grpc::Status status =
            _conn->stub->deleteMediaAlias(&context, grpcRequest, &grpcResponse);
        if (!status.ok()) {
            LOG(ERROR) << "gRPC deleteMediaAlias failed: "
                       << status.error_message();
            res.status = httplib::StatusCode::InternalServerError_500;
            res.set_content(_conn->successFalse, "application/json");
            return;
        }
    }
    res.status = httplib::StatusCode::OK_200;
    nlohmann::json responseJson;
    responseJson["success"] = true;
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