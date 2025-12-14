// #undef CPPHTTPLIB_OPENSSL_SUPPORT
// #undef CPPHTTPLIB_ZLIB_SUPPORT
// #undef CPPHTTPLIB_BROTLI_SUPPORT

#include <absl/log/log.h>
#include <json/json.h>

#include <TgBotWebpage.hpp>
#include <iomanip>
#include <utility>
#include <fstream>

constexpr bool WEBSERVER_INBOUND_VERBOSE = false;

namespace {
std::string getOrEmpty(const std::string &in) {
    return in.empty() ? in : "Empty";
}
}  // namespace

void TgBotWebServerBase::loggerFn(const httplib::Request &req,
                                  const httplib::Response &res) {
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
    auto ret = svr.set_mount_point(Constants::kWebRootNode.data(),
                                   webServerRootPath.string());
    if (!ret) {
        LOG(ERROR) << "Failed to switch mount point to " << webServerRootPath;
        return;
    } else {
        LOG(INFO) << "Web page mount point: " << webServerRootPath.string()
                  << " as root directory";
    }
    svr.Get(Constants::kWebRootNode.data(),
            [](const httplib::Request &req, httplib::Response &res) {
                res.set_redirect(Constants::kAboutPage.data());
            });
    svr.Get(Constants::kAboutPage.data(),
            [this](const httplib::Request &req, httplib::Response &res) {
                callback.showIndex(req, res);
            });
    svr.Post(Constants::kAPIVotesNode.data(),
             [this](const httplib::Request &req, httplib::Response &res) {
                 callback.handleAPIVotes(req, res);
             });
    svr.set_logger(TgBotWebServerBase::loggerFn);
    svr.listen(Constants::kBindToIp.data(), port);
}

void TgBotWebServerBase::stopServer() { svr.stop(); }

TgBotWebServerBase::TgBotWebServerBase(int serverPort,
                                       std::filesystem::path serverPath)
    : port(serverPort),
      callback(this),
      webServerRootPath(std::move(serverPath)) {}

void TgBotWebServerBase::Callbacks::showIndex(const httplib::Request &req,
                                              httplib::Response &res) {
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

void TgBotWebServerBase::Callbacks::handleAPIVotes(const httplib::Request &req,
                                                   httplib::Response &res) {
    constexpr std::string_view kContentType = "Content-Type";
    constexpr std::string_view kContentTypeJson = "application/json";
    if (req.has_header(kContentType.data()) &&
        req.get_header_value(kContentType.data()) == kContentTypeJson) {
        Json::Value document{};
        Json::Reader reader;
        std::string maybeVote;

        if (!reader.parse(req.body, document)) {
            LOG(ERROR) << "Failed to parse JSON";
            res.status = httplib::StatusCode::BadRequest_400;
            return;
        }
        maybeVote = document[Constants::kAPIVotesKey.data()].asString();
        if (maybeVote.empty()) {
            LOG(ERROR) << "Invalid API request: Missing vote value";
            res.status = httplib::StatusCode::BadRequest_400;
            return;
        }
        LOG(INFO) << "API Req vote: " << maybeVote;
        res.status = httplib::StatusCode::OK_200;
    } else {
        LOG(ERROR) << "Invalid API request: Missing vote value";
        res.status = httplib::StatusCode::BadRequest_400;
    }
}