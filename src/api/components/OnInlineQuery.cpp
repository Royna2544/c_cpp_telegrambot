#include <absl/strings/strip.h>

#include <api/components/OnInlineQuery.hpp>

void TgBotApiImpl::OnInlineQueryImpl::onInlineQueryFunction(
    TgBot::InlineQuery::Ptr query) {
    const std::lock_guard m(callback_result_mutex);

    if (queryResults.empty()) {
        return;
    }
    AuthContext::Flags flags = AuthContext::Flags::REQUIRE_USER;
    bool canDoPrivileged = false;
    canDoPrivileged = _auth->isAuthorized(query->from, flags);
    if (!canDoPrivileged) {
        flags |= AuthContext::Flags::PERMISSIVE;
        bool canDoNonPrivileged = _auth->isAuthorized(query->from, flags);
        if (!canDoNonPrivileged) {
            return;  // no permission to answer.
        }
    }
    std::vector<TgBot::InlineQueryResult::Ptr> inlineResults;
    std::ranges::for_each(queryResults, [&query, &inlineResults,
                                         canDoPrivileged](auto&& x) {
        absl::string_view suffix = query->query;
        if (!canDoPrivileged && x.first.enforced) {
            return;  // Skip this.
        }
        if (absl::ConsumePrefix(&suffix, x.first.name)) {
            std::string arg(suffix);
            if (x.first.hasMoreArguments) {
                absl::StripLeadingAsciiWhitespace(&arg);
            }
            auto vec = x.second(std::string_view(suffix.data()));
            inlineResults.insert(inlineResults.end(), vec.begin(), vec.end());
        }
    });
    if (inlineResults.empty()) {
        static int articleCount = 0;
        for (const auto& queryCallbacks : queryResults) {
            auto article = std::make_shared<TgBot::InlineQueryResultArticle>();
            article->id = fmt::format("article-{}", articleCount++);
            article->title =
                fmt::format("Query: {}", queryCallbacks.first.name);
            article->description = queryCallbacks.first.description;
            auto content = std::make_shared<TgBot::InputTextMessageContent>();
            content->messageText = queryCallbacks.first.description;
            article->inputMessageContent = content;
            inlineResults.emplace_back(std::move(article));
        }
    }
    _api->getApi().answerInlineQuery(query->id, inlineResults);
}

TgBotApiImpl::OnInlineQueryImpl::OnInlineQueryImpl(AuthContext* auth,
                                                   TgBotApiImpl::Ptr api)
    : _auth(auth), _api(api) {
    _api->getEvents().onInlineQuery([this](TgBot::InlineQuery::Ptr query) {
        onInlineQueryFunction(std::move(query));
    });
}