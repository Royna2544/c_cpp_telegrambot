#include <api/MessageExt.hpp>
#include <api/components/Restart.hpp>

TgBotApiImpl::RestartCommand::RestartCommand(TgBotApiImpl::Ptr api)
    : _api(api) {
    api->getEvents().onCommand("restart", [this](Message::Ptr message) {
        commandFunction(std::make_shared<MessageExt>(std::move(message)));
    });
}

void TgBotApiImpl::RestartCommand::commandFunction(MessageExt::Ptr message) {
    _api->sendReplyMessage(message->message(), "Not supported in Win32 hosts");
}