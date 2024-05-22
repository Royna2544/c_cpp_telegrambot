// #undef CPPHTTPLIB_OPENSSL_SUPPORT
// #undef CPPHTTPLIB_ZLIB_SUPPORT
// #undef CPPHTTPLIB_BROTLI_SUPPORT

#include <absl/log/log.h>
#include <httplib.h>

#include <TgBotWebpage.hpp>
#include <libos/libfs.hpp>

void TgBotWebServerBase::loggerFn(const httplib::Request &req,
                                  const httplib::Response &res) {
    static std::mutex logger_lock;
    std::lock_guard<std::mutex> lock(logger_lock);
    LOG(INFO) << "=============== Inbound HTTP Request ====================";
    LOG(INFO) << "HTTP request method: " << req.method;
    LOG(INFO) << "From address " << req.remote_addr << ", port "
              << req.remote_port;
    LOG(INFO) << "Status: " << res.status;
    LOG(INFO) << "=========================================================";
}

void TgBotWebServerBase::startServer() {
    const static auto webPageResDir =
        FS::getPathForType(FS::PathType::RESOURCES_WEBPAGE);
    auto ret = svr.set_mount_point("/", webPageResDir.string());
    if (!ret) {
        LOG(ERROR) << "Failed to switch mount point";
        return;
    } else {
        LOG(INFO) << "Web page mount point: " << webPageResDir.string()
                  << " as /";
    }
    svr.Get("/hello", [](const httplib::Request &req, httplib::Response &res) {
        res.set_content("Hello World!", "text/plain");
    });
    svr.Get("/shutdown", [this](const httplib::Request &req,
                                const httplib::Response &res) { svr.stop(); });
    svr.Get("/", [](const httplib::Request &req, httplib::Response &res) {
        std::ifstream stream(webPageResDir / "about.html");
        if (!stream.is_open()) {
            LOG(ERROR) << "Failed to open web page index";
            res.status = 500;
            res.set_content("500 Internal Server Error", "text/plain");
        } else {
            std::string str((std::istreambuf_iterator<char>(stream)),
                            std::istreambuf_iterator<char>());

            res.set_content(str, "text/html");
        }
    });
    svr.set_logger(TgBotWebServerBase::loggerFn);
    svr.listen("localhost", port);
}

void TgBotWebServerBase::stopServer() {
    httplib::Client cli("localhost", port);

    auto res = cli.Get("/shutdown");
    if (res && res->status == httplib::StatusCode::OK_200) {
        LOG(INFO) << "Server stopped";
    }
}