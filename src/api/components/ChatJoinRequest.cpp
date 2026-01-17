#include <absl/strings/strip.h>

#include <algorithm>
#include <api/components/ChatJoinRequest.hpp>
#include <api/types/FormatHelper.hpp>
#include <mutex>

void TgBotApiImpl::ChatJoinRequestImpl::onChatJoinRequestFunction(
    api::types::ChatJoinRequest ptr) {
    if (!AuthContext::isUnderTimeLimit(ptr.date)) {
        // No need to add old requests.
        return;
    }
    templateMarkup->inlineKeyboard.at(0).at(0).callbackData =
        fmt::format("chatjoin_{}_approve", ptr.date);
    templateMarkup->inlineKeyboard.at(0).at(1).callbackData =
        fmt::format("chatjoin_{}_disapprove", ptr.date);
    std::string bio;
    if (ptr.bio) {
        bio = fmt::format("\nTheir Bio: '{}'", ptr.bio.value());
    }
    auto msg = _api->sendMessage(
        *ptr.chat,
        fmt::format("A new chat join request by {}{}", ptr.from, bio),
        templateMarkup);
    if (!msg) {
        LOG(ERROR) << "Failed to send join request message";
        return;
    }
    const std::lock_guard<std::mutex> _(mutex);
    joinReqs.emplace_back(std::move(*msg), std::move(ptr));
}

void TgBotApiImpl::ChatJoinRequestImpl::onCallbackQueryFunction(
    const api::types::CallbackQuery& query) {
    absl::string_view queryData = query.data;
    if (!absl::ConsumePrefix(&queryData, "chatjoin_")) {
        return;
    }
    const std::lock_guard<std::mutex> _(mutex);
    auto reqIt = std::ranges::find_if(joinReqs, [queryData](const auto& req) {
        return absl::StartsWith(queryData, fmt::to_string(req.second.date));
    });
    if (reqIt != joinReqs.end()) {
        if (auto user = _api->getApi().getChatMember(reqIt->first.chat.id,
                                                     query.from.id);
            user &&
            user->status != api::types::ChatMemberAdministrator::STATUS) {
            _api->answerCallbackQuery(query.id,
                                      "Sorry, you are not allowed to");
            return;
        }
        LOG(INFO) << fmt::format("Accepting internal {} by user {}", query.data,
                                 query.from);
        const auto& request = reqIt->second;
        if (!absl::ConsumePrefix(&queryData,
                                 fmt::format("{}_", request.date))) {
            LOG(ERROR) << "Cannot consume date prefix: " << query.data
                       << ". Parsed item: " << queryData;
            return;
        }
        std::string result;
        if (queryData == "approve") {
            LOG(INFO) << fmt::format("Approving {} in chat {}", request.from,
                                     request.chat);
            _api->getApi().approveChatJoinRequest(request.chat->id,
                                                  request.from->id);
            _api->getApi().answerCallbackQuery(query.id, "Approved user");
            result =
                fmt::format("Approved user {} by {}", request.from, query.from);
        } else if (queryData == "disapprove") {
            LOG(INFO) << fmt::format("Disapproved {} in chat {}", request.from,
                                     request.chat);
            _api->getApi().declineChatJoinRequest(request.chat->id,
                                                  request.from->id);
            _api->getApi().answerCallbackQuery(query.id, "Disapproved user");

            result = fmt::format("Disapproved user {} by {}", request.from,
                                 query.from);
        } else {
            LOG(ERROR) << "Invalid payload: " << query.data
                       << ". Parsed item: " << queryData;
            _api->getApi().answerCallbackQuery(query.id,
                                               "Error occurred while parsing");
            return;
        }
        _api->editMessage(reqIt->first, result);
        // Erase the request from the list after handling it.
        joinReqs.erase(reqIt);
    }
}

void TgBotApiImpl::ChatJoinRequestImpl::onChatMemberFunction(
    const api::types::ChatMemberUpdated& update) {
    const std::lock_guard<std::mutex> _(mutex);
    const auto pred = [update](const auto& request) {
        return request.second.chat->id == update.chat->id &&
               request.second.from->id == update.newChatMember->user->id;
    };
    std::ranges::for_each(joinReqs, [pred, this, update](const auto& request) {
        if (pred(request)) {
            LOG(INFO) << fmt::format(
                "Handling externally accepted user {} by {}",
                update.newChatMember->user, update.from);
            _api->deleteMessage(request.first);
        }
    });
    // Erase if the user is now a part of the chat.
    const auto [b, e] = std::ranges::remove_if(joinReqs, pred);
    joinReqs.erase(b, e);
}

TgBotApiImpl::ChatJoinRequestImpl::ChatJoinRequestImpl(TgBotApiImpl::Ptr api)
    : _api(api) {
    // Create keyboard with common markup content
    templateMarkup = api::types::InlineKeyboardMarkup();
    templateMarkup->inlineKeyboard.resize(1);
    templateMarkup->inlineKeyboard.at(0).resize(2);
    templateMarkup->inlineKeyboard.at(0).at(0) =
        api::types::InlineKeyboardButton();
    templateMarkup->inlineKeyboard.at(0).at(0).text = "✅ Approve user";
    templateMarkup->inlineKeyboard.at(0).at(1) =
        api::types::InlineKeyboardButton();
    templateMarkup->inlineKeyboard.at(0).at(1).text = "❌ Kick user";

    _api->getEvents().onChatJoinRequest(
        [this](TgBot::ChatJoinRequest::Ptr query) {
            try {
                onChatJoinRequestFunction(std::move(query));
            } catch (const TgBot::TgException& ex) {
                LOG(ERROR) << "Error in onChatJoinRequest: " << ex.what();
            }
        });
    _api->onCallbackQuery("[builtin::ChatJoinRequest]",
                          [this](const TgBot::CallbackQuery::Ptr& query) {
                              onCallbackQueryFunction(query);
                          });
    _api->getEvents().onChatMember(
        [this](const TgBot::ChatMemberUpdated::Ptr& update) {
            try {
                onChatMemberFunction(update);
            } catch (const TgBot::TgException& ex) {
                LOG(ERROR) << "Error in onChatMember: " << ex.what();
            }
        });
}