#include <chrono>

#include "ManagedThreads.hpp"
#include "api/TgBotApi.hpp"
#include "global_handlers/SpamBlock.hpp"
#include "trivial_helpers/fruit_inject.hpp"

class SocketServiceImpl final : public ThreadRunner {
   public:
    struct Url {
        std::string url;
    };
    struct Service;
    struct Impl;

   private:
    TgBotApi* api_;
    SpamBlockBase* spamBlock_;
    Url* url_;
    std::unique_ptr<Service> service_;
    std::unique_ptr<Impl> impl_;

    void runFunction(const std::stop_token& token) override;

    void onPreStop() override;

   public:
    APPLE_EXPLICIT_INJECT(SocketServiceImpl(TgBotApi* api,
                                            SpamBlockBase* spamBlock,
                                            Url* url));
    ~SocketServiceImpl() override;
};