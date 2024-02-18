#include <Authorization.h>
#include <Logging.h>
#include <NamespaceImport.h>
#include <SpamBlock.h>

#include <map>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <utility>
#include "tgbot/TgException.h"

using TgBot::ChatPermissions;

#ifdef SOCKET_CONNECTION
CtrlSpamBlock gSpamBlockCfg = CTRL_LOGGING_ONLY_ON;
#endif

using UserType = std::pair<TgBot::Chat::Ptr, std::vector<Message::Ptr>>;
using SpamMapT = std::map<TgBot::User::Ptr, std::vector<Message::Ptr>>;
using buffer_iterator_t = std::map<TgBot::Chat::Ptr, ChatHandle>::const_iterator;

std::string toUserName(const TgBot::User::Ptr bro) {
    std::string username = bro->firstName;
    if (!bro->lastName.empty())
        username += ' ' + bro->lastName;
    return '\'' + username + '\'';
}

std::string toChatName(const TgBot::Chat::Ptr ch) {
    return '\'' + ch->title + '\'';
}

template <class Type>
struct _FindIdIt {
    template <class Container>
    static auto find(Container& c, std::function<typename Type::Ptr(typename Container::const_reference)> fn,
                   typename Type::Ptr t) {
        return std::find_if(c.begin(), c.end(), [=](const auto& it) {
            return fn(it)->id == t->id;
        });
    }
};

using findChatIt = _FindIdIt<Chat>;
using findUserIt = _FindIdIt<User>;

static std::string commonMsgdataFn(const Message::Ptr &m) {
    if (m->sticker)
        return m->sticker->fileUniqueId;
    else if (m->animation)
        return m->animation->fileUniqueId;
    else
        return m->text;
}

static bool isEntryOverThreshold(const SpamMapT::value_type& t, const size_t threshold) {
    const size_t kEntryValue = t.second.size();
    const bool isOverThreshold = kEntryValue >= threshold;
    if (isOverThreshold)
        LOG_D("Note: Value %zu is over threshold %zu", kEntryValue, threshold);
    return isOverThreshold;
}

static void _logSpamDetectCommon(const SpamMapT::value_type& t, const char* name) {
    LOG_I("Spam detected for user %s, filtered by %s", toUserName(t.first).c_str(), name);
}

static void _deleteAndMuteCommon(const Bot& bot, const buffer_iterator_t& handle, const SpamMapT::value_type& t,
                                 const size_t threshold, const char* name) {
                                     // Initial set - all false set
    static auto perms = std::make_shared<ChatPermissions>();
    if (isEntryOverThreshold(t, threshold)) {
        _logSpamDetectCommon(t, name);
    
        for (const auto &msg : t.second) {
            try {
                bot.getApi().deleteMessage(handle->first->id, msg->messageId);
            } catch (const TgBot::TgException &) {
            }
        }
        try {
            bot.getApi().restrictChatMember(handle->first->id, t.first->id,
                                            perms, 5 * 60);
        } catch (const TgBot::TgException &e) {
            LOG_W("Cannot mute user %s in chat %s: %s", toUserName(t.first).c_str(), 
                    toChatName(handle->first).c_str(), e.what());
        }
    }
}

#ifdef SOCKET_CONNECTION
static void deleteAndMute(const Bot &bot, buffer_iterator_t handle,
                          const SpamMapT &map, const size_t threshold, const char* name) {
    // Initial set - all false set
    auto perms = std::make_shared<ChatPermissions>();
    for (const auto &mapmsg : map) {
        switch (gSpamBlockCfg) {
            case TgBotCommandData::CTRL_ON: {
                _deleteAndMuteCommon(bot, handle, mapmsg, threshold, name);
                break;
            }
            case TgBotCommandData::CTRL_LOGGING_ONLY_ON:
                if (isEntryOverThreshold(mapmsg, threshold))
                    _logSpamDetectCommon(mapmsg, name);
                break;
            default:
                break;
        };
    }
}
#else
static void deleteAndMute(const Bot &bot, buffer_iterator_t handle,
                          const SpamMapT &map, const size_t threshold, const char* name) {
    for (const auto &mapmsg : map) {
        _deleteAndMuteCommon(bot, handle, mapmsg, threshold, name);
        _logSpamDetectCommon(mapmsg, name);
    }
}
#endif

static void spamDetectFunc(const Bot &bot, buffer_iterator_t handle) {
    SpamMapT MaxSameMsgMap, MaxMsgMap;
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

    deleteAndMute(bot, handle, MaxSameMsgMap, 3, "MaxSameMsg");
    deleteAndMute(bot, handle, MaxMsgMap, 5, "MaxMsg");
}

static void spamBlockerFn(const Bot& bot, std::shared_ptr<SpamBlockBuffer> buf) {
    auto& buffer = buf->buffer;
    auto& buffer_sub = buf->buffer_sub;
    while (buf->kRun) {
        {
            const std::lock_guard<std::mutex> _(buf->m);
            if (buffer_sub.size() > 0) {
                auto its = buffer_sub.begin();
                const auto chatNameStr = toChatName(its->first);
                const auto chatName = chatNameStr.c_str();
                while (its != buffer_sub.end()) {
                    const auto it = findChatIt::find(buffer, 
                        [](const auto &it) { return it.first; }, its->first);
                    if (it == buffer.end()) {
                        its = buffer_sub.erase(its);
                        continue;
                    }
                    LOG_V("Chat: %s, MsgCount: %d", chatName, its->second);
                    if (its->second >= 5) {
                        LOG_D("Launching spamdetect for %s", chatName);
                        spamDetectFunc(bot, it);
                    }
                    buffer.erase(it);
                    its->second = 0;
                    ++its;
                }
            }
        }
        std_sleep_s(10);
    }
}

void spamBlocker(const Bot &bot, const Message::Ptr &message, std::shared_ptr<SpamBlockBuffer> buf) {
    static std::once_flag once;

#ifdef SOCKET_CONNECTION
    // Global cfg
    if (gSpamBlockCfg == CTRL_OFF) return;
#endif
    // I'm allowed always
    if (Authorized(message, true)) return;
    // Do not track older msgs and consider it as spam.
    if (std::time(0) - message->date > 15) return;
    // We care GIF, sticker, text spams only
    if (!message->animation && message->text.empty() && !message->sticker)
        return;

    buf->kRun = true;
    std::call_once(once, [buf, &bot] {
        buf->kThreadP = std::thread(&spamBlockerFn, std::cref(bot), buf);
    });
    {
        const std::lock_guard<std::mutex> _(buf->m);
        auto& buffer = buf->buffer;
        auto& buffer_sub = buf->buffer_sub;
        auto bufferIt = findChatIt::find(buffer, [](const auto& it) { return it.first; },
                                          message->chat);
        if (bufferIt != buffer.end()) {
            const auto bufferUserIt = findUserIt::find(bufferIt->second, [](const auto& it) { 
                return it.first;
            }, message->from);

            if (bufferUserIt != bufferIt->second.end()) {
                bufferUserIt->second.emplace_back(message);
            } else {
                bufferIt->second[message->from].emplace_back(message);
            }
        } else {
            buffer[message->chat][message->from].emplace_back(message);
        }
        const auto bufferSubIt = findChatIt::find(buffer_sub,
            [](const auto& it) { return it.first; },  message->chat);
        if (bufferSubIt != buffer_sub.end()) {
            ++bufferSubIt->second;
        } else {
            ++buffer_sub[message->chat];
        }
    }
}
