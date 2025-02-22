
#include "CompilerInTelegram.hpp"
#include "DurationPoint.hpp"
#include "api/TgBotApi.hpp"
#include "popen_wdt.h"

class CompilerInTgBotInterface : public CompilerInTg::Interface {
   public:
    ~CompilerInTgBotInterface() override = default;
    void onExecutionStarted(const std::string_view& command) override;
    void onExecutionFinished(const std::string_view& command, const popen_watchdog_exit_t& exit) override;
    void onErrorStatus(absl::Status status) override;
    void onResultReady(const std::string& text) override;
    void onWdtTimeout() override;

    explicit CompilerInTgBotInterface(
        TgBotApi::CPtr api, const StringResLoader::PerLocaleMap* locale,
        MessageExt::Ptr requestedMessage);

   private:
    TgBotApi::CPtr botApi;
    MessageExt::Ptr requestedMessage;
    Message::Ptr sentMessage;
    MilliSecondDP timePoint;
    std::stringstream output;
    const StringResLoader::PerLocaleMap* _locale;
};
