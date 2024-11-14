#include <absl/log/log.h>
#include <trivial_helpers/_tgbot.h>

#include <api/components/OnMyChatMember.hpp>

#include "tgbot/types/ChatMemberAdministrator.h"
#include "tgbot/types/ChatMemberBanned.h"
#include "tgbot/types/ChatMemberMember.h"
#include "tgbot/types/ChatMemberRestricted.h"

template <>
struct fmt::formatter<TgBotApiImpl::OnMyChatMemberImpl::BotState>
    : formatter<std::string_view> {
    using BotState = TgBotApiImpl::OnMyChatMemberImpl::BotState;
    // parse is inherited from formatter<string_view>.
    auto format(BotState c,
                format_context& ctx) const -> format_context::iterator {
        string_view name = "unknown";
        switch (c) {
            case TgBotApiImpl::OnMyChatMemberImpl::BotState::UNKNOWN:
                break;
            case TgBotApiImpl::OnMyChatMemberImpl::BotState::IS_MEMBER:
                name = "member";
                break;
            case TgBotApiImpl::OnMyChatMemberImpl::BotState::IS_ADMIN:
                name = "admin";
                break;
            case TgBotApiImpl::OnMyChatMemberImpl::BotState::IS_RESTRICTED:
                name = "restricted";
                break;
            case TgBotApiImpl::OnMyChatMemberImpl::BotState::IS_BANNED:
                name = "banned";
                break;
        }
        return formatter<string_view>::format(name, ctx);
    }
};

TgBotApiImpl::OnMyChatMemberImpl::BotState
TgBotApiImpl::OnMyChatMemberImpl::parseState(
    const TgBot::ChatMember::Ptr& update) {
    auto status = update->status;
    if (status == TgBot::ChatMemberAdministrator::STATUS) {
        return BotState::IS_ADMIN;
    } else if (status == TgBot::ChatMemberBanned::STATUS) {
        return BotState::IS_BANNED;
    } else if (status == TgBot::ChatMemberMember::STATUS) {
        return BotState::IS_MEMBER;
    } else if (status == TgBot::ChatMemberRestricted::STATUS) {
        return BotState::IS_RESTRICTED;
    }
    LOG(ERROR) << "Unhandled status: " << status;
    return BotState::UNKNOWN;
}

TgBotApiImpl::OnMyChatMemberImpl::OnMyChatMemberImpl(TgBotApiImpl::Ptr api)
    : _api(api) {
    reporters.emplace_back(std::make_unique<LoggingReport>());
    if (auto owner = api->_provider->database->getOwnerUserId(); owner) {
        reporters.emplace_back(
            std::make_unique<MessageReport>(api, owner.value()));
    } else {
        LOG(WARNING) << "Owner cannot be resolved, using logging system only "
                        "for reports";
    }
    api->getEvents().onMyChatMember(
        [this](const TgBot::ChatMemberUpdated::Ptr& update) {
            onMyChatMemberFunction(update);
        });
}

void TgBotApiImpl::OnMyChatMemberImpl::onMyChatMemberFunction(
    const TgBot::ChatMemberUpdated::Ptr& update) {
    for (const auto& rep : reporters) {
        rep->onStatusChange(update->chat, parseState(update->oldChatMember),
                            parseState(update->newChatMember));
    }
}

TgBotApiImpl::OnMyChatMemberImpl::MessageReport::MessageReport(
    TgBotApiImpl::Ptr api, const UserId ownerId)
    : _api(api), _ownerId(ownerId) {}

void TgBotApiImpl::OnMyChatMemberImpl::MessageReport::onStatusChange(
    const Chat::Ptr& chat, const BotState oldStatus, const BotState newStatus) {
    if (oldStatus == newStatus && oldStatus == BotState::IS_ADMIN) {
        _api->sendMessage(
            _ownerId,
            fmt::format("In chat {}, admin permission has changed", chat));
    } else {
        _api->sendMessage(
            _ownerId, fmt::format("In chat {}, status changed from {} to {}",
                                  chat, oldStatus, newStatus));
    }
}

void TgBotApiImpl::OnMyChatMemberImpl::LoggingReport::onStatusChange(
    const Chat::Ptr& chat, const BotState oldStatus, const BotState newStatus) {
    if (oldStatus == newStatus && oldStatus == BotState::IS_ADMIN) {
        LOG(INFO) << fmt::format("In chat {}, admin permission has changed",
                                 chat);
    } else {
        LOG(INFO) << fmt::format("In chat {}, status changed from {} to {}",
                                 chat, oldStatus, newStatus);
    }
}