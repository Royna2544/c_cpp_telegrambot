#include <Logging.h>
#include <NamespaceImport.h>

#include <optional>
#include <regex>
#include <string>

#include "BotReplyMessage.h"
#include "tgbot/tools/StringTools.h"

using std::regex_constants::ECMAScript;
using std::regex_constants::format_first_only;
using std::regex_constants::format_sed;
using std::regex_constants::icase;
using std::regex_constants::match_not_null;
using std::regex_constants::syntax_option_type;

// Matches sed command with subsitute command and g or i flags
static const std::regex kSedReplaceCommandRegex(R"(^s\/.+\/.+(\/(g|i|ig|gi))?$)");
// Matches sed command with delete command, with regex on deleting expression
static const std::regex kSedDeleteCommandRegex(R"(^\/.+\/d$)");

template <typename T>
class OptionalWrapper {
    std::optional<T> val;

   public:
    OptionalWrapper<T>(const std::optional<T> _val) : val(_val) {}
    OptionalWrapper<T>() : val(std::nullopt) {}

    bool has_value() const {
        return val.has_value();
    }
    T value() const {
        return val.value();
    }
    T operator*() const {
        return value();
    }
    operator bool() const {
        return has_value();
    }
    OptionalWrapper<T> operator|=(OptionalWrapper<T>&& other) {
        if (this != &other) {
            if (other.has_value() && !has_value()) {
                val = std::move(other.val);
            }
        }
        return *this;
    }
};

static std::vector<std::string> matchAndSplit(const Bot& bot, const Message::Ptr msg, const std::regex& regex) {
    std::string& text = msg->text;
    if (std::regex_match(text, regex)) {
        if (msg->replyToMessage && !msg->replyToMessage->text.empty()) {
            return StringTools::split(text, '/');
        } else {
            bot_sendReplyMessage(bot, msg, "Reply to a text message");
        }
    }
    return {};
}

static std::optional<std::regex> safeConstructRegex(const Bot& bot, const Message::Ptr& msg,
                                                    const std::string& regexstr, const syntax_option_type flags) {
    try {
        return std::regex(regexstr, flags);
    } catch (const std::regex_error& e) {
        bot_sendReplyMessage(bot, msg, "Failed to parse regex (if it is) in '" + regexstr + "': " + e.what());
        return std::nullopt;
    }
}

static OptionalWrapper<std::string> doRegexReplaceCommand(const Bot& bot, const Message::Ptr& msg) {
    std::regex src;
    std::string dest;
    bool global = false;  // g flag in sed
    auto kRegexFlags = ECMAScript;
    auto kRegexMatchFlags = format_sed | match_not_null;
    auto args = matchAndSplit(bot, msg, kSedReplaceCommandRegex);

    // s/aaaa/bbbb/g (split to 4 strings)
    // s/aaaa/bbbb   (split to 3 strings)
    // Else invalid
    if (auto len = args.size(); len == 3 || len == 4) {
        if (len == 4) {
            const auto& opt = args[3];
            // Due to the above regex match, it should be either none, i, g, ig, gi
            global = opt.find('g') != std::string::npos;
            if (opt.find('i') != std::string::npos)
                kRegexFlags |= icase;
        }
        if (auto regex = safeConstructRegex(bot, msg, args[1], kRegexFlags); regex.has_value()) {
            src = regex.value();
            dest = args[2];

            if (!global)
                kRegexMatchFlags |= format_first_only;

            LOG_D("src: '%s' dest: '%s' global: %d icase: %d", args[1].c_str(), args[2].c_str(),
                  global, kRegexFlags & icase);
            try {
                return {std::regex_replace(msg->replyToMessage->text, src, dest, kRegexMatchFlags)};
            } catch (const std::regex_error& e) {
                bot_sendReplyMessage(bot, msg, std::string() + "Exception while executing doRegex: " + e.what());
            }
        }
    }

    return {std::nullopt};
}

static OptionalWrapper<std::string> doRegexDeleteCommand(const Bot& bot, const Message::Ptr& msg) {
    auto args = matchAndSplit(bot, msg, kSedDeleteCommandRegex);
    // /aaaa/d
    if (args.size() == 3) {
        if (auto regex = safeConstructRegex(bot, msg, args[1], ECMAScript); regex.has_value()) {
            std::stringstream kInStream(msg->replyToMessage->text), kOutStream;
            std::string line;
            LOG_D("regexstr: '%s'", args[1].c_str());
            while (std::getline(kInStream, line)) {
                if (!std::regex_search(line, regex.value(), format_sed | match_not_null)) {
                    kOutStream << line << std::endl;
                }
            }
            return {kOutStream.str()};
        }
    }
    return {std::nullopt};
}

void processRegEXCommand(const Bot& bot, const Message::Ptr& msg) {
    OptionalWrapper<std::string> ret;
    ret |= doRegexReplaceCommand(bot, msg);
    ret |= doRegexDeleteCommand(bot, msg);
    if (ret)
        bot_sendReplyMessage(bot, msg, *ret);
}
