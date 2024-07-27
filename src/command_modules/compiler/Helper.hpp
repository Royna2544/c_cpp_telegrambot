#include <memory>

#include "CompilerInTelegram.hpp"
#include "DurationPoint.hpp"
#include "TgBotWrapper.hpp"

class CompilerInTgBotInterface : public CompilerInTg::Interface {
   public:
    ~CompilerInTgBotInterface() override = default;
    void onExecutionStarted(const std::string_view& command) override;
    void onExecutionFinished(const std::string_view& command) override;
    void onErrorStatus(absl::Status status) override;
    void onResultReady(const std::string& text) override;
    void onWdtTimeout() override;

    explicit CompilerInTgBotInterface(std::shared_ptr<TgBotApi> api,
                                      Message::Ptr requestedMessage);

   private:
    std::shared_ptr<TgBotApi> botApi;
    Message::Ptr requestedMessage;
    Message::Ptr sentMessage;
    DurationPoint timePoint;
    std::stringstream output;
};