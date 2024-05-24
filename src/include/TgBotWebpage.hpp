#include <httplib.h>

#include <filesystem>
#include <functional>
#include <string_view>

#include "CStringLifetime.h"
#include "SingleThreadCtrl.h"
#include "initcalls/Initcall.hpp"

class TgBotWebServerBase {
   public:
    void startServer();
    void stopServer() const;

    explicit TgBotWebServerBase(int serverPort, std::filesystem::path serverPath);

    static void loggerFn(const httplib::Request &req,
                         const httplib::Response &res);
    struct Constants {
        static constexpr const std::string_view kWebRootNode = "/";
        static constexpr const std::string_view kWebShutdownNode = "/shutdown";
        static constexpr const std::string_view kBindToIp = "0.0.0.0";
        static constexpr const std::string_view kLocalHostname = "localhost";
    };
    struct Callbacks {
        using type = std::function<void(const httplib::Request &req,
                                        const httplib::Response &res)>;
        void shutdown(const httplib::Request &req, httplib::Response &res) const;
        void showIndex(const httplib::Request &req, httplib::Response &res) const;
        explicit Callbacks(TgBotWebServerBase *server) : server(server) {}

       private:
        TgBotWebServerBase *server;
    } callback;

   private:
    int port;
    httplib::Server svr{};
    std::filesystem::path webServerRootPath;
};

class TgBotWebServer : public SingleThreadCtrlRunnable,
                       InitCall,
                       TgBotWebServerBase {
   public:
    explicit TgBotWebServer(int serverPort);

    using InitCall::initWrapper;
    void runFunction() override;

    void doInitCall() override;

    const CStringLifetime getInitCallName() const override;
};