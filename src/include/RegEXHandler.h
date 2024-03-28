#pragma once

#include <tgbot/Bot.h>
#include <tgbot/types/Message.h>

#include <optional>
#include <regex>
#include <string>
#include <vector>

#include "BotClassBase.h"
#include "InstanceClassBase.hpp"
#include "initcalls/BotInitcall.hpp"
#include "OnAnyMessageRegister.hpp"

using std::regex_constants::syntax_option_type;
using TgBot::Bot;
using TgBot::Message;

template <typename T>
class OptionalWrapper {
    std::optional<T> val;

   public:
    OptionalWrapper(const std::optional<T> _val) : val(_val) {}
    OptionalWrapper() : val(std::nullopt) {}

    bool has_value() const { return val.has_value(); }
    T value() const { return val.value(); }
    T operator*() const { return value(); }
    operator bool() const { return has_value(); }
    OptionalWrapper<T> operator|=(OptionalWrapper<T>&& other) {
        if (this != &other) {
            if (other.has_value() && !has_value()) {
                val = std::move(other.val);
            }
        }
        return *this;
    }
};

struct RegexHandlerBase {
    RegexHandlerBase() = default;
    virtual ~RegexHandlerBase() = default;

    virtual void onRegexCreationFailed(const Message::Ptr& which,
                                       const std::string& what,
                                       const std::regex_error& why) {
        onRegexOperationFailed(which, why);
    }
    virtual void onRegexOperationFailed(const Message::Ptr& which,
                                        const std::regex_error& why) {}
    virtual void onRegexProcessed(const Message::Ptr& from,
                                  const std::string& processedData) {}

    void processRegEXCommand(const Message::Ptr& regexCommand,
                             const std::string& text);

    friend struct RegexHandlerTest;

   private:
    std::vector<std::string> matchRegexAndSplit(const std::string& text,
                                                const std::regex& regex);
    std::optional<std::regex> constructRegex(const std::string& regexstr,
                                             const Message::Ptr& message,
                                             const syntax_option_type flags);

    OptionalWrapper<std::string> doRegexReplaceCommand(
        const Message::Ptr& regexCommand, const std::string& text);
    OptionalWrapper<std::string> doRegexDeleteCommand(
        const Message::Ptr& regexCommand, const std::string& text);
};

struct RegexHandler : public RegexHandlerBase, BotClassBase, BotInitCall, InstanceClassBase<RegexHandler> {
    explicit RegexHandler(const Bot& bot) : BotClassBase(bot) {}
    RegexHandler() = delete;
    virtual ~RegexHandler() override = default;

    void onRegexCreationFailed(const Message::Ptr& which,
                               const std::string& what,
                               const std::regex_error& why) override;
    void onRegexOperationFailed(const Message::Ptr& which,
                                const std::regex_error& why) override;
    void onRegexProcessed(const Message::Ptr& from,
                          const std::string& processedData) override;
    void processRegEXCommandMessage(const Message::Ptr& message);

    void doInitCall(Bot& bot) override {
        OnAnyMessageRegisterer::getInstance().registerCallback(
            [this](const Bot& bot, const Message::Ptr& message) {
                processRegEXCommandMessage(message);
            });
    }
    const char* getInitCallName() const override {
        return "Add regexhandler anymessage callback";
    }
};
