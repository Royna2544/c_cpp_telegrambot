#include <Authorization.h>
#include <BotReplyMessage.h>
#include <Logging.h>
#include <SpamBlock.h>
#include <socket/TgBotSocket.h>

#include <map>
#include <mutex>
#include <utility>

#include "tgbot/types/Chat.h"

using std::chrono_literals::operator""s;
using TgBot::ChatPermissions;

#ifdef SOCKET_CONNECTION
CtrlSpamBlock gSpamBlockCfg = CTRL_ON;
#endif

std::string SpamBlockBase::toUserName(const User::Ptr bro) {
    std::string username = bro->firstName;
    if (!bro->lastName.empty())
        username += ' ' + bro->lastName;
    return '\'' + username + '\'';
}

std::string SpamBlockBase::toChatName(const Chat::Ptr ch) {
    return '\'' + ch->title + '\'';
}

template <class Container, class Type>
typename Container::iterator _findItImpl(Container &c, std::function<typename Type::Ptr(typename Container::const_reference)> fn, typename Type::Ptr t) {
    return std::find_if(c.begin(), c.end(), [=](const auto &it) {
        return fn(it)->id == t->id;
    });
}

template <class Container>
typename Container::iterator findChatIt(Container &c, std::function<Chat::Ptr(typename Container::const_reference)> fn, Chat::Ptr t) {
    return _findItImpl<Container, Chat>(c, fn, t);
}

template <class Container>
typename Container::iterator findUserIt(Container &c, std::function<User::Ptr(typename Container::const_reference)> fn, User::Ptr t) {
    return _findItImpl<Container, User>(c, fn, t);
}

std::string SpamBlockBase::commonMsgdataFn(const Message::Ptr &m) {
    if (m->sticker)
        return m->sticker->fileUniqueId;
    else if (m->animation)
        return m->animation->fileUniqueId;
    else
        return m->text;
}

bool SpamBlockBase::isEntryOverThreshold(PerChatHandle::const_reference t, const size_t threshold) {
    const size_t kEntryValue = t.second.size();
    const bool isOverThreshold = kEntryValue >= threshold;
    if (isOverThreshold)
        LOG_D("Note: Value %zu is over threshold %zu", kEntryValue, threshold);
    return isOverThreshold;
}

void SpamBlockBase::_logSpamDetectCommon(PerChatHandle::const_reference t, const char *name) {
    LOG_I("Spam detected for user %s, filtered by %s", toUserName(t.first).c_str(), name);
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
        std::apply([&](const auto &first, const auto &second) {
            MaxMsgMap.emplace(first, second);
        }, perUser);
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
        auto mostCommonIt = std::max_element(commonMap.begin(), commonMap.end(),
                                             [](const auto &lhs, const auto &rhs) {
                                                 return lhs.second.size() < rhs.second.size();
                                             });
        MaxSameMsgMap.emplace(pair.first, mostCommonIt->second);
    }

    takeAction(handle, MaxSameMsgMap, 3, "MaxSameMsg");
    takeAction(handle, MaxMsgMap, 5, "MaxMsg");
}

void SpamBlockBase::runFunction(void) {
    while (kRun) {
        {
            const std::lock_guard<std::mutex> _(buffer_m);
            if (buffer_sub.size() > 0) {
                auto its = buffer_sub.begin();
                const auto chatNameStr = toChatName(its->first);
                const auto chatName = chatNameStr.c_str();
                while (its != buffer_sub.end()) {
                    const auto it = findChatIt(
                        buffer, [](const auto &it) { return it.first; },
                        its->first);
                    if (it == buffer.end()) {
                        its = buffer_sub.erase(its);
                        continue;
                    }
                    LOG_V("Chat: %s, MsgCount: %d", chatName, its->second);
                    if (its->second >= 5) {
                        LOG_D("Launching spamdetect for %s", chatName);
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
    if (gSpamBlockCfg == CTRL_OFF) return;
#endif
    // I'm allowed always
    if (Authorized(message, 0)) return;
    // Do not track older msgs and consider it as spam.
    if (!isMessageUnderTimeLimit(message)) return;
    // We care GIF, sticker, text spams only, or if it isn't fowarded msg
    if ((!message->animation && message->text.empty() && !message->sticker) || message->forwardFrom)
        return;

    std::call_once(once, [this] {
        run();
    });

    {
        const std::lock_guard<std::mutex> _(buffer_m);
        auto bufferIt = findChatIt(
            buffer, [](const auto &it) { return it.first; },
            message->chat);
        if (bufferIt != buffer.end()) {
            const auto bufferUserIt = findUserIt(
                bufferIt->second, [](const auto &it) {
                    return it.first;
                },
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
            buffer_sub,
            [](const auto &it) { return it.first; }, message->chat);
        if (bufferSubIt != buffer_sub.end()) {
            ++bufferSubIt->second;
        } else {
            ++buffer_sub[message->chat];
        }
    }
}

void SpamBlockManager::handleUserAndMessagePair(PerChatHandleConstRef e, OneChatIterator it,
                                                const size_t threshold, const char *name) {
    bool enforce = false;
#ifdef SOCKET_CONNECTION
    switch (gSpamBlockCfg) {
        case CTRL_ENFORCE:
            enforce = true;
            [[fallthrough]];
        case CTRL_ON: {
            _deleteAndMuteCommon(it, e, threshold, name, enforce);
            break;
        }
        case CTRL_LOGGING_ONLY_ON:
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

void SpamBlockManager::_deleteAndMuteCommon(const OneChatIterator &handle, PerChatHandle::const_reference t,
                                            const size_t threshold, const char *name, const bool mute) {
    // Initial set - all false set
    static auto perms = std::make_shared<ChatPermissions>();
    if (isEntryOverThreshold(t, threshold)) {
        _logSpamDetectCommon(t, name);

        bot_sendMessage(_bot, handle->first->id, "Spam detected @" + t.first->username);
        for (const auto &msg : t.second) {
            try {
                _bot.getApi().deleteMessage(handle->first->id, msg->messageId);
            } catch (const TgBot::TgException &e) {
                LOG_V("Error deleting message: %s", e.what());
            }
        }

        if (mute) {
            LOG_I("Try mute user %s in chat %s", toUserName(t.first).c_str(),
                  toChatName(handle->first).c_str());
            try {
                _bot.getApi().restrictChatMember(handle->first->id, t.first->id,
                                                 perms, 5 * 60);
            } catch (const TgBot::TgException &e) {
                LOG_W("Cannot mute user %s in chat %s: %s", toUserName(t.first).c_str(),
                      toChatName(handle->first).c_str(), e.what());
            }
        }
    }
}