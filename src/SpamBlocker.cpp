#include <Authorization.h>
#include <CStringLifetime.h>
#include <absl/log/log.h>
#include <internal/_std_chrono_templates.h>
#include <internal/_tgbot.h>
#include <tgbot/types/Chat.h>

#include <InstanceClassBase.hpp>
#include <SpamBlock.hpp>
#include <algorithm>
#include <memory>
#include <mutex>

#include "ManagedThreads.hpp"
#include "TgBotWrapper.hpp"
#include "Types.h"

using std::chrono_literals::operator""s;
using TgBot::ChatPermissions;

template <class Container, class Type>
typename Container::iterator _findItImpl(
    Container &c,
    std::function<typename Type::Ptr(typename Container::const_reference)> fn,
    typename Type::Ptr t) {
    return std::ranges::find_if(
        c, [=](const auto &it) { return fn(it)->id == t->id; });
}

template <class Container>
typename Container::iterator findChatIt(
    Container &c,
    std::function<Chat::Ptr(typename Container::const_reference)> fn,
    Chat::Ptr t) {
    return _findItImpl<Container, Chat>(c, fn, t);
}

template <class Container>
typename Container::iterator findUserIt(
    Container &c,
    std::function<User::Ptr(typename Container::const_reference)> fn,
    User::Ptr t) {
    return _findItImpl<Container, User>(c, fn, t);
}

std::string SpamBlockBase::commonMsgdataFn(const Message::Ptr &m) {
    if (m->sticker) {
        return m->sticker->fileUniqueId;
    } else if (m->animation) {
        return m->animation->fileUniqueId;
    } else {
        return m->text;
    }
}

bool SpamBlockBase::isEntryOverThreshold(PerChatHandle::const_reference t,
                                         const size_t threshold) {
    const size_t kEntryValue = t.second.size();
    const bool isOverThreshold = kEntryValue >= threshold;
    DLOG_IF(INFO, isOverThreshold)
        << "Note: Value " << kEntryValue << " is over threshold " << threshold;
    return isOverThreshold;
}

void SpamBlockBase::_logSpamDetectCommon(PerChatHandle::const_reference t,
                                         const char *name) {
    LOG(INFO) << "Spam detected for user " << UserPtr_toString(t.first)
              << ", filtered by " << name;
}

void SpamBlockBase::takeAction(OneChatIterator it, const PerChatHandle &map,
                               const size_t threshold, const char *name) {
    for (const auto &mapmsg : map) {
        handleUserAndMessagePair(mapmsg, it, threshold, name);
    }
}

void SpamBlockBase::spamDetectFunc(OneChatIterator handle) {
    PerChatHandle MaxSameMsgMap, MaxMsgMap;
    // By most msgs sent by that user
    for (const auto &perUser : handle->second) {
        std::apply(
            [&](const auto &first, const auto &second) {
                MaxMsgMap.emplace(first, second);
            },
            perUser);
    }

    for (const auto &pair : handle->second) {
        std::multimap<std::string, Message::Ptr> byMsgContent;
        for (const auto &obj : pair.second) {
            byMsgContent.emplace(commonMsgdataFn(obj), obj);
        }
        std::unordered_map<std::string, std::vector<Message::Ptr>> commonMap;
        for (const auto &cnt : byMsgContent) {
            commonMap[cnt.first].emplace_back(cnt.second);
        }
        // Find the most common value
        auto mostCommonIt =
            std::max_element(commonMap.begin(), commonMap.end(),
                             [](const auto &lhs, const auto &rhs) {
                                 return lhs.second.size() < rhs.second.size();
                             });
        MaxSameMsgMap.emplace(pair.first, mostCommonIt->second);
    }

    takeAction(handle, MaxSameMsgMap, sMaxSameMsgThreshold, "MaxSameMsg");
    takeAction(handle, MaxMsgMap, sMaxMsgThreshold, "MaxMsg");
}

void SpamBlockBase::runFunction() {
    while (kRun) {
        {
            const std::lock_guard<std::mutex> _(buffer_m);
            if (buffer_sub.size() > 0) {
                auto its = buffer_sub.begin();
                const CStringLifetime chatName = ChatPtr_toString(its->first);
                while (its != buffer_sub.end()) {
                    const auto it = findChatIt(
                        buffer, [](const auto &it) { return it.first; },
                        its->first);
                    if (it == buffer.end()) {
                        its = buffer_sub.erase(its);
                        continue;
                    }
                    if (its->second >= sSpamDetectThreshold) {
                        LOG(INFO) << "Launching spamdetect for chat "
                                  << std::quoted(chatName.get());
                        spamDetectFunc(it);
                    }
                    buffer.erase(it);
                    its->second = 0;
                    ++its;
                }
            }
        }
        delayUnlessStop(10s);
    }
}

void SpamBlockBase::addMessage(const Message::Ptr &message) {
    static std::once_flag once;

#ifdef SOCKET_CONNECTION
    // Global cfg
    if (spamBlockConfig == CtrlSpamBlock::CTRL_OFF) {
        return;
    }
#endif
    // I'm allowed always
    if (AuthContext::getInstance()->isAuthorized(message, 0)) {
        return;
    }

    // Ignore old messages
    if (!AuthContext::isMessageUnderTimeLimit(message)) {
        return;
    }

    // We care GIF, sticker, text spams only, or if it isn't fowarded msg
    if ((!message->animation && message->text.empty() && !message->sticker) ||
        message->forwardOrigin) {
        return;
    }
    // Bot's PM is not a concern
    if (message->chat->type == TgBot::Chat::Type::Private) {
        return;
    }

    std::call_once(once, [this] { run(); });

    {
        const std::lock_guard<std::mutex> _(buffer_m);
        auto bufferIt = findChatIt(
            buffer, [](const auto &it) { return it.first; }, message->chat);
        if (bufferIt != buffer.end()) {
            const auto bufferUserIt = findUserIt(
                bufferIt->second, [](const auto &it) { return it.first; },
                message->from);

            if (bufferUserIt != bufferIt->second.end()) {
                bufferUserIt->second.emplace_back(message);
            } else {
                bufferIt->second[message->from].emplace_back(message);
            }
        } else {
            buffer[message->chat][message->from].emplace_back(message);
        }
        const auto bufferSubIt = findChatIt(
            buffer_sub, [](const auto &it) { return it.first; }, message->chat);
        if (bufferSubIt != buffer_sub.end()) {
            ++bufferSubIt->second;
        } else {
            ++buffer_sub[message->chat];
        }
    }
}

void SpamBlockManager::handleUserAndMessagePair(PerChatHandleConstRef e,
                                                OneChatIterator it,
                                                const size_t threshold,
                                                const char *name) {
    bool enforce = false;
#ifdef SOCKET_CONNECTION
    switch (spamBlockConfig) {
        case CtrlSpamBlock::CTRL_ENFORCE:
            enforce = true;
            [[fallthrough]];
        case CtrlSpamBlock::CTRL_ON: {
            _deleteAndMuteCommon(it, e, threshold, name, enforce);
            break;
        }
        case CtrlSpamBlock::CTRL_LOGGING_ONLY_ON:
            if (isEntryOverThreshold(e, threshold))
                _logSpamDetectCommon(e, name);
            break;
        default:
            break;
    };
#else
    _deleteAndMuteCommon(it, e, threshold, name, enforce);
#endif
}

void SpamBlockManager::_deleteAndMuteCommon(const OneChatIterator &handle,
                                            PerChatHandle::const_reference t,
                                            const size_t threshold,
                                            const char *name, const bool mute) {
    const auto wrapper = _api;
    // Initial set - all false set
    static auto perms = std::make_shared<ChatPermissions>();
    if (isEntryOverThreshold(t, threshold)) {
        const CStringLifetime userstr = UserPtr_toString(t.first);
        const CStringLifetime chatstr = ChatPtr_toString(handle->first);

        _logSpamDetectCommon(t, name);

        wrapper->sendMessage(handle->first->id,
                             "Spam detected @" + t.first->username);
        std::vector<MessageId> message_ids;
        std::ranges::for_each(t.second, [&message_ids](auto &&messageIn) {
            message_ids.emplace_back(messageIn->messageId);
        });
        try {
            wrapper->deleteMessages(handle->first->id, message_ids);
        } catch (const TgBot::TgException &e) {
            DLOG(INFO) << "Error deleting message: " << e.what();
        }
        if (mute) {
            LOG(INFO) << "Try mute user " << userstr.get() << " in chat "
                      << chatstr.get();
            try {
                wrapper->muteChatMember(handle->first->id, t.first->id, perms,
                                        to_secs(kMuteDuration).count());
            } catch (const TgBot::TgException &e) {
                LOG(WARNING)
                    << "Cannot mute user " << userstr.get() << " in chat "
                    << chatstr.get() << ": " << e.what();
            }
        }
    }
}

void SpamBlockManager::doInitCall() {
    _api->registerCallback([this](const auto, const Message::Ptr &message) {
        static auto spamMgr =
            ThreadManager::getInstance()
                ->createController<ThreadManager::Usage::SPAMBLOCK_THREAD,
                                   SpamBlockManager>(_api);
        spamMgr->addMessage(message);
    });
}

DECLARE_CLASS_INST(SpamBlockManager);