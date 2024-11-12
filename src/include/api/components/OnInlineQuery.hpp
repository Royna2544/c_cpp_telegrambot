#include <api/TgBotApiImpl.hpp>
#include "Authorization.hpp"

class TgBotApiImpl::OnInlineQueryImpl {
    // Protect callbacks
    std::mutex callback_result_mutex;
    std::map<InlineQuery, InlineCallback> queryResults;
    AuthContext* _auth;
    TgBotApiImpl::Ptr _api;

    void onInlineQueryFunction(TgBot::InlineQuery::Ptr query);

   public:
    OnInlineQueryImpl(AuthContext* auth, TgBotApiImpl::Ptr api);
    
    void add(InlineQuery query, TgBot::InlineQueryResult::Ptr result) {
        const std::lock_guard _(callback_result_mutex);
        queryResults[std::move(query)] =
            [result = std::move(result)](const std::string_view /*unused*/) {
                return std::vector{result};
            };
    }
    void add(InlineQuery query, InlineCallback result) {
        const std::lock_guard _(callback_result_mutex);
        queryResults[std::move(query)] = std::move(result);
    }
    void remove(const std::string_view key) {
        const std::lock_guard _(callback_result_mutex);
        for (auto it = queryResults.begin(); it != queryResults.end(); ++it) {
            if (it->first.name == static_cast<std::string>(key)) {
                queryResults.erase(it);
                break;
            }
        }
    }
};