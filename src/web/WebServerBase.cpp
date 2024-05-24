// #undef CPPHTTPLIB_OPENSSL_SUPPORT
// #undef CPPHTTPLIB_ZLIB_SUPPORT
// #undef CPPHTTPLIB_BROTLI_SUPPORT

#include <absl/log/log.h>
#include <httplib.h>

#include <TgBotWebpage.hpp>
#include <iomanip>
#include <libos/libfs.hpp>
#include <utility>

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
    DLOG(INFO) << "Remote Info";
    DLOG(INFO) << "Address: " << req.remote_addr
               << " Port: " << req.remote_port;
    DLOG(INFO) << "Host: " << req.get_header_value("Host");
    DLOG(INFO) << "User-Agent: " << req.get_header_value("User-Agent");
    DLOG(INFO) << "Referer: " << getOrEmpty(req.get_header_value("Referer"));
    DLOG(INFO) << "Accept: " << req.get_header_value("Accept");
    DLOG(INFO) << "Requested filepath: " << std::quoted(req.path);
    DLOG(INFO) << "Status: " << res.status;
    DLOG(INFO) << "=========================================================";
}

void TgBotWebServerBase::startServer() {
    auto ret = svr.set_mount_point("/", webServerRootPath.string());
    if (!ret) {
        LOG(ERROR) << "Failed to switch mount point";
        return;
    } else {
        LOG(INFO) << "Web page mount point: " << webServerRootPath.string()
                  << " as root directory";
    }
    svr.Get(Constants::kWebShutdownNode.data(),
            [this](const httplib::Request &req, httplib::Response &res) {
                callback.shutdown(req, res);
            });
    svr.Get(Constants::kWebRootNode.data(),
            [this](const httplib::Request &req, httplib::Response &res) {
                callback.showIndex(req, res);
            });
    svr.set_logger(TgBotWebServerBase::loggerFn);
    svr.listen(Constants::kBindToIp.data(), port);
}

void TgBotWebServerBase::stopServer() const {
    httplib::Client cli(Constants::kLocalHostname.data(), port);

    auto res = cli.Get(Constants::kWebShutdownNode.data());
    if (res && res->status == httplib::StatusCode::OK_200) {
        LOG(INFO) << "Server stopped";
    } else {
        LOG(ERROR) << "Failed to stop server: Code "
                   << (res ? std::to_string(res->status) : "(res is null)");
    }
}

TgBotWebServerBase::TgBotWebServerBase(int serverPort,
                                       std::filesystem::path serverPath)
    : port(serverPort),
      callback(this),
      webServerRootPath(std::move(serverPath)) {}

void TgBotWebServerBase::Callbacks::shutdown(const httplib::Request &req,
                                             httplib::Response &res) const {
    server->svr.stop();
    res.status = httplib::StatusCode::OK_200;
    res.set_content("OK", "text/plain");
}

void TgBotWebServerBase::Callbacks::showIndex(const httplib::Request &req,
                                              httplib::Response &res) const {
    std::ifstream stream(server->webServerRootPath / "about.html");
    if (!stream.is_open()) {
        LOG(ERROR) << "Failed to open web page index";
        res.status = httplib::StatusCode::InternalServerError_500;
        res.set_content("500 Internal Server Error", "text/plain");
    } else {
        std::string str((std::istreambuf_iterator<char>(stream)),
                        std::istreambuf_iterator<char>());

        res.set_content(str, "text/html");
    }
}