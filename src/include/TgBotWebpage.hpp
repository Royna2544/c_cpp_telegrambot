#include <httplib.h>

#include "CStringLifetime.h"
#include "SingleThreadCtrl.h"
#include "initcalls/Initcall.hpp"

class TgBotWebServerBase {
   public:
    void startServer();
    void stopServer();

    explicit TgBotWebServerBase(int serverPort) : port(serverPort) {}

    static void loggerFn(const httplib::Request &req, const httplib::Response &res);
   private:
    int port;
    httplib::Server svr{};
};

class TgBotWebServer : public SingleThreadCtrlRunnable,
                       InitCall,
                       TgBotWebServerBase {
   public:
    explicit TgBotWebServer(int serverPort) : TgBotWebServerBase(serverPort) {}

    using InitCall::initWrapper;
    void runFunction() override;

    void doInitCall() override;

    const CStringLifetime getInitCallName() const override;
};