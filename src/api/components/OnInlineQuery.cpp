#include <absl/log/log.h>
#include <absl/strings/strip.h>
#include <tgbot/TgException.h>

#include <api/components/ModuleExecutionContext.hpp>
#include <api/components/ModuleManagement.hpp>
#include <api/components/OnInlineQuery.hpp>
#include <atomic>
#include <cstdint>
#include <exception>
#include <memory>
#include <utility>
#include <vector>

namespace {
struct InlineInvocation {
    std::string owner;
    std::shared_ptr<RefLock::SharedLease> moduleLease;
    TgBotApi::InlineCallback callback;
    std::string argument;

    ~InlineInvocation() {
        // A module-defined std::function manager must be destroyed while its
        // DSO is still protected from dlclose.
        callback = {};
        moduleLease.reset();
    }

    std::vector<TgBot::InlineQueryResult::Ptr> invoke() const {
        if (owner.empty()) {
            return callback(argument);
        }
        module_execution::Scope execution(owner);
        return callback(argument);
    }
};
}  // namespace

void TgBotApiImpl::OnInlineQueryImpl::onInlineQueryFunction(
    TgBot::InlineQuery::Ptr query) {
    AuthContext::AccessLevel flags = AuthContext::AccessLevel::AdminUser;
    const bool canDoPrivileged = _auth->isAuthorized(query->from, flags);
    if (!canDoPrivileged) {
        flags = AuthContext::AccessLevel::User;
        const bool canDoNonPrivileged = _auth->isAuthorized(query->from, flags);
        if (!canDoNonPrivileged) {
            return;  // no permission to answer.
        }
    }

    // Keep module leases alive until both the Telegram request and destruction
    // of callback-produced result objects have completed. Declare the result
    // vector after this one so it is destroyed first during stack unwinding.
    std::vector<std::shared_ptr<InlineInvocation>> invocations;
    std::vector<TgBot::InlineQueryResult::Ptr> inlineResults;
    std::vector<InlineQuery> advertisedQueries;
    {
        const std::lock_guard lock(mutex);
        if (queryResults.empty()) {
            return;
        }

        invocations.reserve(queryResults.size());
        advertisedQueries.reserve(queryResults.size());
        for (const auto& [registration, callback] : queryResults) {
            if (!canDoPrivileged && registration.enforced) {
                continue;
            }

            advertisedQueries.push_back(registration);

            absl::string_view suffix = query->query;
            if (!absl::ConsumePrefix(&suffix, registration.name)) {
                continue;
            }

            auto invocation = std::make_shared<InlineInvocation>();
            invocation->owner = registration.command;
            if (!invocation->owner.empty()) {
                if (!_api->kModuleLoader) {
                    continue;
                }
                invocation->moduleLease =
                    _api->kModuleLoader->acquireExecutionLease(
                        invocation->owner);
                if (!invocation->moduleLease) {
                    continue;
                }
            }

            try {
                // Acquire the owner lease before copying the std::function:
                // its target's copy constructor may itself reside in the DSO.
                if (invocation->owner.empty()) {
                    invocation->callback = callback;
                } else {
                    module_execution::Scope execution(invocation->owner);
                    invocation->callback = callback;
                }
                invocation->argument = std::string(suffix);
                if (registration.hasMoreArguments) {
                    absl::StripLeadingAsciiWhitespace(&invocation->argument);
                }
                invocations.emplace_back(std::move(invocation));
            } catch (const std::exception& error) {
                LOG(ERROR) << "Could not snapshot inline-query callback owned "
                              "by "
                           << registration.command << ": " << error.what();
            } catch (...) {
                LOG(ERROR) << "Unknown exception while snapshotting inline-"
                              "query callback owned by "
                           << registration.command;
            }
        }
    }

    for (const auto& invocation : invocations) {
        try {
            auto results = invocation->invoke();
            inlineResults.insert(inlineResults.end(), results.begin(),
                                 results.end());
        } catch (const TgBot::TgException& error) {
            LOG(ERROR) << "Telegram exception in inline-query callback owned "
                          "by "
                       << invocation->owner << ": " << error.what();
        } catch (const std::exception& error) {
            LOG(ERROR) << "Exception in inline-query callback owned by "
                       << invocation->owner << ": " << error.what();
        } catch (...) {
            LOG(ERROR) << "Unknown exception in inline-query callback owned "
                          "by "
                       << invocation->owner;
        }
    }

    if (inlineResults.empty()) {
        static std::atomic_uint64_t articleCount = 0;
        for (const auto& registration : advertisedQueries) {
            auto article = std::make_shared<TgBot::InlineQueryResultArticle>();
            article->id = fmt::format(
                "article-{}",
                articleCount.fetch_add(1, std::memory_order_relaxed));
            article->title = fmt::format("Query: {}", registration.name);
            article->description = registration.description;
            auto content = std::make_shared<TgBot::InputTextMessageContent>();
            content->messageText = registration.description;
            article->inputMessageContent = content;
            inlineResults.emplace_back(std::move(article));
        }
    }

    try {
        _api->getApi().answerInlineQuery(query->id, inlineResults);
    } catch (const TgBot::TgException& error) {
        LOG(ERROR) << "Could not answer inline query: " << error.what();
    } catch (const std::exception& error) {
        LOG(ERROR) << "Exception while answering inline query: "
                   << error.what();
    } catch (...) {
        LOG(ERROR) << "Unknown exception while answering inline query";
    }
}

void TgBotApiImpl::OnInlineQueryImpl::onUnload(const std::string_view command) {
    const std::lock_guard lock(mutex);
    for (auto it = queryResults.begin(); it != queryResults.end();) {
        if (it->first.command == command) {
            DLOG(INFO) << "Removing inline query handler for " << command;
            it = queryResults.erase(it);
        } else {
            ++it;
        }
    }
}

void TgBotApiImpl::OnInlineQueryImpl::onReload(const std::string_view command) {
}

TgBotApiImpl::OnInlineQueryImpl::OnInlineQueryImpl(AuthContext* auth,
                                                   TgBotApiImpl::Ptr api)
    : _auth(auth), _api(api) {
    _api->getEvents().onInlineQuery([this](TgBot::InlineQuery::Ptr query) {
        onInlineQueryFunction(std::move(query));
    });
    _api->addCommandListener(this);
}
