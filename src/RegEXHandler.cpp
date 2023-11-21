#include <Logging.h>
#include <NamespaceImport.h>

#include <regex>
#include <string>

#include "BotReplyMessage.h"
#include "tgbot/tools/StringTools.h"

struct RegEXContext {
    std::regex src;
    std::string dest;
    bool global;
};

using std::regex_constants::ECMAScript;
using std::regex_constants::format_first_only;
using std::regex_constants::format_sed;
using std::regex_constants::icase;
using std::regex_constants::match_not_null;

static std::string doRegex(const RegEXContext* ctx, const std::string& text) {
    std::string s = text;
    auto flags = format_sed | match_not_null;
    if (!ctx->global)
        flags |= format_first_only;
    return std::regex_replace(text, ctx->src, ctx->dest, flags);
}

void processRegEXCommand(const Bot& bot, const Message::Ptr& msg) {
    // Matches sed command with subsitute command and g or i flags
    static std::regex kSedCommandRegex(R"(s\/.+\/.+(\/(g|i|ig|gi))?)");
    std::string& text = msg->text;
    if (std::regex_match(text, kSedCommandRegex)) {
        if (msg->replyToMessage && !msg->replyToMessage->text.empty()) {
            auto vec = StringTools::split(text, '/');
            if (vec.size() == 3 || vec.size() == 4) {
                struct RegEXContext ctx {};
                auto flags = ECMAScript;

                if (vec.size() == 4) {
                    const auto& opt = vec[3];
                    // Due to the above regex match, it should be either none, i, g, ig, gi
                    ctx.global = opt.find('g') != std::string::npos;
                    if (opt.find('i') != std::string::npos)
                        flags |= icase;
                }
                try {
                    ctx.src = std::regex(vec[1], flags);
                } catch (const std::regex_error& e) {
                    bot_sendReplyMessage(bot, msg, "Failed to parse regex (if it is) in '" + vec[1] + "': " + e.what());
                    return;
                }
                ctx.dest = vec[2];
                LOG_D("src: '%s' dest: '%s' global: %d icase: %d", vec[1].c_str(), vec[2].c_str(),
                      ctx.global, flags & icase);
                try {
                    auto result = doRegex(&ctx, msg->replyToMessage->text);
                    bot_sendReplyMessage(bot, msg->replyToMessage, result);
                } catch (const std::regex_error& e) {
                    bot_sendReplyMessage(bot, msg, std::string() + "Exception while executing doRegex: " + e.what());
                }
            }
        } else {
            bot_sendReplyMessage(bot, msg, "Reply to a text message");
        }
    }
}
