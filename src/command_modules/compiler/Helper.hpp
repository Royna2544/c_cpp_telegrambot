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
        InstanceClassBase<TgBotApi>::const_pointer_type api,
        MessageExt::Ptr requestedMessage);

   private:
    InstanceClassBase<TgBotApi>::const_pointer_type botApi;
    MessageExt::Ptr requestedMessage;
    Message::Ptr sentMessage;
    DurationPoint timePoint;
    std::stringstream output;
};