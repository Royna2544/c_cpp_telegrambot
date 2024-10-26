#include <memory>

#include "CompilerInTelegram.hpp"
#include "DurationPoint.hpp"
#include "InstanceClassBase.hpp"
#include "api/TgBotApi.hpp"

class CompilerInTgBotInterface : public CompilerInTg::Interface {
   public:
    ~CompilerInTgBotInterface() override = default;
    void onExecutionStarted(const std::string_view& command) override;
    void onExecutionFinished(const std::string_view& command) override;
    void onErrorStatus(absl::Status status) override;
    void onResultReady(const std::string& text) override;
    void onWdtTimeout() override;

    explicit CompilerInTgBotInterface(
        TgBotApi::CPtr api, const StringResLoaderBase::LocaleStrings* locale,
        MessageExt::Ptr requestedMessage);

   private:
    TgBotApi::CPtr botApi;
    MessageExt::Ptr requestedMessage;
    Message::Ptr sentMessage;
    DurationPoint timePoint;
    std::stringstream output;
    const StringResLoaderBase::LocaleStrings* _locale;
};