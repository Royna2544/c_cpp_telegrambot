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

using std::regex_constants::format_sed;
using std::regex_constants::format_first_only;
using std::regex_constants::match_not_null;

static std::string doRegex(const RegEXContext* ctx, const std::string& text) {
    std::string s = text;
    auto flags = format_sed | match_not_null;
    if (!ctx->global)
        flags |= format_first_only;
    return std::regex_replace(text, ctx->src, ctx->dest, flags);
}

void processRegEXCommand(const Bot& bot, const Message::Ptr& msg) {
    static std::regex kSedCommandRegex(R"(s\/.+\/.+(\/g)?)");
    std::string& text = msg->text;
    if (std::regex_match(text, kSedCommandRegex)) {
        if (msg->replyToMessage && !msg->replyToMessage->text.empty()) {
            auto vec = StringTools::split(text, '/');
            if (vec.size() == 3 || vec.size() == 4) {
                struct RegEXContext ctx {};

                try {
                    ctx.src = std::regex(vec[1]);
                } catch (const std::regex_error& e) {
                    bot_sendReplyMessage(bot, msg, "Failed to parse regex (if it is) in '" + vec[1] + "': " + e.what());
                    return;
                }

                ctx.dest = vec[2];
                ctx.global = vec.size() == 4 && vec[3] == "g";
                LOG_D("src: '%s' dest: '%s' global: %d", vec[1].c_str(), vec[2].c_str(), ctx.global);
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
