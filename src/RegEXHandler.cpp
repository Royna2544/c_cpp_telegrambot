#include <BotReplyMessage.h>
#include <RegEXHandler.h>
#include <absl/log/log.h>

#include <ios>
#include <optional>
#include <regex>
#include <string>

#include "InstanceClassBase.hpp"
#include "StringToolsExt.h"

using std::regex_constants::ECMAScript;
using std::regex_constants::format_first_only;
using std::regex_constants::format_sed;
using std::regex_constants::icase;
using std::regex_constants::match_not_null;

// Matches sed command with subsitute command and g or i flags
static const std::regex kSedReplaceCommandRegex(
    R"(^s\/.+\/.+(\/(g|i|ig|gi))?$)");
// Matches sed command with delete command, with regex on deleting expression
static const std::regex kSedDeleteCommandRegex(R"(^\/.+\/d$)");

std::vector<std::string> RegexHandlerBase::matchRegexAndSplit(
    const std::string& text, const std::regex& regex) {
    if (std::regex_match(text, regex)) return StringTools::split(text, '/');
    return {};
}

std::optional<std::regex> RegexHandlerBase::constructRegex(
    const std::string& regexstr, const Message::Ptr& message,
    const syntax_option_type flags) {
    try {
        return std::regex(regexstr, flags);
    } catch (const std::regex_error& e) {
        onRegexCreationFailed(message, regexstr, e);
        return std::nullopt;
    }
}

OptionalWrapper<std::string> RegexHandlerBase::doRegexReplaceCommand(
    const Message::Ptr& regexCommand, const std::string& desttext) {
    std::regex src;
    std::string dest;
    bool global = false;  // g flag in sed
    auto kRegexFlags = ECMAScript;
    auto kRegexMatchFlags = format_sed | match_not_null;
    auto args = matchRegexAndSplit(regexCommand->text, kSedReplaceCommandRegex);

    // s/aaaa/bbbb/g (split to 4 strings)
    // s/aaaa/bbbb   (split to 3 strings)
    // Else invalid
    if (auto len = args.size(); len == 3 || len == 4) {
        if (len == 4) {
            const auto& opt = args[3];
            // Due to the above regex match, it should be either none, i, g, ig,
            // gi
            global = opt.find('g') != std::string::npos;
            if (opt.find('i') != std::string::npos) kRegexFlags |= icase;
        }
        if (auto regex = constructRegex(args[1], regexCommand, kRegexFlags);
            regex.has_value()) {
            src = regex.value();
            dest = args[2];

            if (!global) kRegexMatchFlags |= format_first_only;

            DLOG(INFO) << "src: '" << args[1] << "' dest: '" << args[2]
                       << std::boolalpha << "' global: " << global
                       << " icase: " << (kRegexFlags & icase);
            try {
                return {
                    std::regex_replace(desttext, src, dest, kRegexMatchFlags)};
            } catch (const std::regex_error& e) {
                onRegexOperationFailed(regexCommand, e);
            }
        }
    }

    return {std::nullopt};
}

OptionalWrapper<std::string> RegexHandlerBase::doRegexDeleteCommand(
    const Message::Ptr& regexCommand, const std::string& text) {
    auto args = matchRegexAndSplit(regexCommand->text, kSedDeleteCommandRegex);
    // /aaaa/d
    if (args.size() == 3) {
        if (auto regex = constructRegex(args[1], regexCommand, ECMAScript);
            regex.has_value()) {
            std::stringstream kInStream(text), kOutStream;
            std::string line, out;
            DLOG(INFO) << "regexstr: '" << args[1] << "'";
            while (std::getline(kInStream, line)) {
                if (!std::regex_search(line, regex.value(),
                                       format_sed | match_not_null)) {
                    kOutStream << line << std::endl;
                }
            }
            out = kOutStream.str();
            TrimStr(out);
            return {out};
        }
    }
    return {std::nullopt};
}

void RegexHandlerBase::processRegEXCommand(const Message::Ptr& srcstr,
                                           const std::string& dststr) {
    OptionalWrapper<std::string> ret;
    ret |= doRegexReplaceCommand(srcstr, dststr);
    ret |= doRegexDeleteCommand(srcstr, dststr);
    if (ret) {
        onRegexProcessed(srcstr, *ret);
    }
}

void RegexHandler::onRegexCreationFailed(const Message::Ptr& message,
                                         const std::string& what,
                                         const std::regex_error& why) {
    bot_sendReplyMessage(
        _bot, message,
        "Failed to parse regex (if it is) in '" + what + "': " + why.what());
}
void RegexHandler::onRegexOperationFailed(const Message::Ptr& which,
                                          const std::regex_error& why) {
    bot_sendReplyMessage(
        _bot, which,
        std::string() + "Exception while executing doRegex: " + why.what());
}
void RegexHandler::onRegexProcessed(const Message::Ptr& from,
                                    const std::string& processedData) {
    bot_sendReplyMessage(_bot, from->replyToMessage, processedData);
}
void RegexHandler::processRegEXCommandMessage(const Message::Ptr& message) {
    if (message->replyToMessage && !message->replyToMessage->text.empty())
        processRegEXCommand(message, message->replyToMessage->text);
}

void RegexHandler::doInitCall() {
    OnAnyMessageRegisterer::getInstance()->registerCallback(
        [this](const Bot&  /*bot*/, const Message::Ptr& message) {
            processRegEXCommandMessage(message);
        });
}

DECLARE_CLASS_INST(RegexHandler);