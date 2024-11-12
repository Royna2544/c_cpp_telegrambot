#include <api/components/UnknownCommand.hpp>

TgBotApiImpl::OnUnknownCommandImpl::OnUnknownCommandImpl(
    TgBotApiImpl::Ptr api) {
    api->getEvents().onUnknownCommand([api](const Message::Ptr& message) {
        const auto ext = std::make_shared<MessageExt>(message);
        if (ext->get_or<MessageAttrs::BotCommand>({}).target !=
            api->getBotUser()->username) {
            return;  // ignore, unless explicitly targetted this bot.
        }
        LOG(INFO) << "Unknown command: " << message->text;
    });
}