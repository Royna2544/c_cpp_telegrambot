#include <memory>
#include <mutex>
#include <utility>

#include "RegEXHandler.hpp"
#include "TgBotWrapper.hpp"

class RegexHandlerInterface : public RegexHandlerBase::Interface {
   public:
    void onError(const absl::Status& status) override {
        _api->sendReplyMessage(
            _message,
            "RegexHandler has encountered an error: " + status.ToString());
    }
    void onSuccess(const std::string& result) override {
        _api->sendReplyMessage(_message->replyToMessage, result);
    }
    void setMessage(Message::Ptr message) { _message = std::move(message); }

    explicit RegexHandlerInterface(std::shared_ptr<TgBotApi> api)
        : _api(std::move(api)) {}

   private:
    std::shared_ptr<TgBotApi> _api;
    Message::Ptr _message;
};

void RegexHandler::doInitCall() {
    static std::once_flag once;
    static std::shared_ptr<RegexHandlerInterface> intf;
    static std::shared_ptr<RegexHandlerBase> handler;

    TgBotWrapper::getInstance()->onAnyMessage([](ApiPtr api,
                                                     MessagePtr message) {
        std::call_once(once, [api]() {
            intf = std::make_shared<RegexHandlerInterface>(api);
            handler = std::make_shared<RegexHandlerBase>(intf);
        });

        if (message->has<MessageExt::Attrs::IsReplyMessage,
                         MessageExt::Attrs::ExtraText>() &&
            message
                ->replyToMessage_has<MessageExt::Attrs::ExtraText>()) {
            intf->setMessage(message);
            handler->setContext({.regexCommand = message->text,
                                 .text = message->replyToMessage->text});

            handler->process();
        }
        return TgBotWrapper::AnyMessageResult::Handled;
    });
}