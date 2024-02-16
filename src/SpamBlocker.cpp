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

using TgBot::Chat;
using TgBot::ChatPermissions;
using TgBot::User;

#ifdef SOCKET_CONNECTION
CtrlSpamBlock gSpamBlockCfg = CTRL_LOGGING_ONLY_ON;
#endif

using ChatHandle = std::map<TgBot::User::Ptr, std::vector<Message::Ptr>>;
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

static void deleteAndMute(const Bot &bot, buffer_iterator_t handle,
                          const SpamMapT &map, const size_t threshold) {
    // Initial set - all false set
    auto perms = std::make_shared<ChatPermissions>();
    for (const auto &mapmsg : map) {
#ifdef SOCKET_CONNECTION
        switch (gSpamBlockCfg) {
            case TgBotCommandData::CTRL_ON: {
#endif
                if (mapmsg.second.size() >= threshold) {
#ifdef SOCKET_CONNECTION
                    if (gSpamBlockCfg == CTRL_ON) {
#endif
                        for (const auto &msg : mapmsg.second) {
                            try {
                                bot.getApi().deleteMessage(handle->first->id, msg->messageId);
                            } catch (const TgBot::TgException &ignored) {
                            }
                        }
                        try {
                            bot.getApi().restrictChatMember(handle->first->id, mapmsg.first->id,
                                                            perms, 5 * 60);
                        } catch (const std::exception &e) {
                            LOG_W("Cannot mute user %s in chat %s: %s", toUserName(mapmsg.first).c_str(), 
                                toChatName(handle->first).c_str(), e.what());
                        }
                    }
#ifdef SOCKET_CONNECTION
                }
                [[fallthrough]];
            }
            case TgBotCommandData::CTRL_LOGGING_ONLY_ON:
#endif
                LOG_I("Spam detected for user %s", toUserName(mapmsg.first).c_str());
#ifdef SOCKET_CONNECTION
                break;
            default:
                break;
        };
#endif
    }
}

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
        LOG_D("Chat: %s, User: %s, maxIt: %zu", toChatName(handle->first).c_str(),
              toUserName(pair.first).c_str(), mostCommonIt->second.size());
    }

    deleteAndMute(bot, handle, MaxSameMsgMap, 3);
    deleteAndMute(bot, handle, MaxMsgMap, 5);
}

struct SpamBlockBuffer {
    std::map<Chat::Ptr, ChatHandle> buffer;
    std::map<Chat::Ptr, int> buffer_sub;
    std::mutex m;  // Protect buffer, buffer_sub
};

static void spamBlockerFn(const Bot& bot, SpamBlockBuffer& buf) {
    auto& buffer = buf.buffer;
    auto& buffer_sub = buf.buffer_sub;
    while (true) {
        {
            const std::lock_guard<std::mutex> _(buf.m);
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
                    LOG_V("Chat: %s, Count: %d", chatName, its->second);
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

void spamBlocker(const Bot &bot, const Message::Ptr &message) {
    static SpamBlockBuffer buf;

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

    static bool init = false;
    if (!init) {
        std::thread(&spamBlockerFn, std::cref(bot), std::ref(buf)).detach();
        init = true;
    }
    {
        const std::lock_guard<std::mutex> _(buf.m);
        auto bufferIt = findChatIt::find(buf.buffer, [](const auto& it) { return it.first; },
                                          message->chat);
        if (bufferIt != buf.buffer.end()) {
            const auto bufferUserIt = findUserIt::find(bufferIt->second, [](const auto& it) { 
                return it.first;
            }, message->from);

            if (bufferUserIt != bufferIt->second.end()) {
                bufferUserIt->second.emplace_back(message);
            } else {
                bufferIt->second[message->from].emplace_back(message);
            }
        } else {
            buf.buffer[message->chat][message->from].emplace_back(message);
        }
        const auto bufferSubIt = findChatIt::find(buf.buffer_sub,
            [](const auto& it) { return it.first; },  message->chat);
        if (bufferSubIt != buf.buffer_sub.end()) {
            ++bufferSubIt->second;
        } else {
            ++buf.buffer_sub[message->chat];
        }
    }
}
