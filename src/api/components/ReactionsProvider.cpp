#include <api/components/ReactionsProvider.hpp>
#include <regex>

#include "api/TgBotApi.hpp"
#include "api/TgBotApiImpl.hpp"
#include "tgbot/types/ReactionTypeEmoji.h"

TgBotApiImpl::ReactionsProvider::ReactionsProvider(TgBotApi* apiImpl)
    : _apiImpl(apiImpl) {
    _apiImpl->onAnyMessage(
        [this](TgBotApi::CPtr /*api*/, const Message::Ptr& message) {
            return this->onAnyMessageFunction(message);
        });
}

static void addReaction(std::vector<TgBot::ReactionType::Ptr>& reactions,
                        const std::string& emoji) {
    auto e = std::make_shared<TgBot::ReactionTypeEmoji>();
    e->emoji = emoji;
    reactions.emplace_back(e);
}

template <typename T>
void apply(std::vector<TgBot::ReactionType::Ptr>& reactions,
           const Message::Ptr& message, const T& filter) = delete;

// Filter by file name
struct FilterFilename {
    std::string emoji;
    std::string match;
};

template <>
void apply<FilterFilename>(std::vector<TgBot::ReactionType::Ptr>& reactions,
                           const Message::Ptr& message,
                           const FilterFilename& filter) {
    if (message->document && message->document->fileName == filter.match) {
        addReaction(reactions, filter.emoji);
    }
}

// Filter by user
struct FilterUser {
    std::string emoji;
    UserId userId;
};

template <>
void apply<FilterUser>(std::vector<TgBot::ReactionType::Ptr>& reactions,
                       const Message::Ptr& message, const FilterUser& filter) {
    if (message->from && message->from->id == filter.userId) {
        addReaction(reactions, filter.emoji);
    }
}

// Filter by message text
struct FilterText {
    std::string emoji;
    std::regex match;
};

template <>
void apply<FilterText>(std::vector<TgBot::ReactionType::Ptr>& reactions,
                       const Message::Ptr& message, const FilterText& filter) {
    if (message->text && std::regex_search(*message->text, filter.match)) {
        addReaction(reactions, filter.emoji);
    }
}

TgBotApi::AnyMessageResult
TgBotApiImpl::ReactionsProvider::onAnyMessageFunction(Message::Ptr message) {
    std::vector<TgBot::ReactionType::Ptr> reactions;
    if (!reactions.empty()) {
        _apiImpl->setMessageReaction(message, reactions, false);
    }
    return TgBotApi::AnyMessageResult::Handled;
}