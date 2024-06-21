#include <httplib.h>

#include <filesystem>
#include <functional>
#include <string_view>

#include "CStringLifetime.h"
#include "ManagedThreads.hpp"
#include "initcalls/Initcall.hpp"

class TgBotWebServerBase {
   public:
    void startServer();
    void stopServer();

    explicit TgBotWebServerBase(int serverPort, std::filesystem::path serverPath);

    static void loggerFn(const httplib::Request &req,
                         const httplib::Response &res);
    struct Constants {
        static constexpr const std::string_view kWebRootNode = "/";
        static constexpr const std::string_view kAboutPage = "/about.html";
        static constexpr const std::string_view kAPIVotesNode = "/api/votes";
        static constexpr const std::string_view kAPIVotesKey = "votes";
        static constexpr const std::string_view kBindToIp = "0.0.0.0";
        static constexpr const std::string_view kLocalHostname = "localhost";
    };
    struct Callbacks {
        using type = std::function<void(const httplib::Request &req,
                                        const httplib::Response &res)>;
        void showIndex(const httplib::Request &req, httplib::Response &res);
        static void handleAPIVotes(const httplib::Request &req, httplib::Response &res);
        explicit Callbacks(TgBotWebServerBase *server) : server(server) {}

       private:
        TgBotWebServerBase *server;
    } callback;

   private:
    int port;
    httplib::Server svr {};
    std::filesystem::path webServerRootPath;
};

class TgBotWebServer : public ManagedThreadRunnable,
                       InitCall,
                       TgBotWebServerBase {
   public:
    explicit TgBotWebServer(int serverPort);

    using InitCall::initWrapper;
    void runFunction() override;

    void doInitCall() override;

    const CStringLifetime getInitCallName() const override;
};