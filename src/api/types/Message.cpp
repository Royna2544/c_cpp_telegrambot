#include <absl/log/log.h>
#include <absl/strings/ascii.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>

#include <api/types/Message.hpp>
#include <memory>

namespace tgbot_api {

void Message::parseText(SplitMessageText how) {
    // Parse reply message if it exists
    if (replyToMessage) {
        parsedReplyMessage = replyToMessage;
        if (parsedReplyMessage) {
            parsedReplyMessage->parseText(how);
        }
    }

    // Empty message won't need parsing
    if (!text) {
        return;
    }

    // Initially, extraArgs is full text
    extraArgs = text.value();

    // Try to find botcommand entity
    const auto botCommandEnt =
        std::ranges::find_if(entities, [](const auto& entity) {
            return entity->type ==
                       tgbot_api::MessageEntity::Type::BotCommand &&
                   entity->offset == 0;
        });

    // I believe entity must be sent here.
    if (botCommandEnt != entities.end() && text->front() == '/') {
        const auto entry = *botCommandEnt;
        // Grab /start@username
        extraArgs = text->substr(entry->length);
        absl::StripLeadingAsciiWhitespace(&extraArgs);
        command.emplace();
        std::pair<std::string, std::string> kCommandSplit =
            absl::StrSplit(text->substr(1, entry->length), "@");
        command->name = kCommandSplit.first;
        command->target = kCommandSplit.second;
        absl::StripTrailingAsciiWhitespace(&command->target);
    }

    if (extraArgs.size() != 0) {
        switch (how) {
            case SplitMessageText::ByWhitespace:
                arguments =
                    absl::StrSplit(extraArgs, ' ', absl::SkipWhitespace());
                break;
            case SplitMessageText::ByComma:
                arguments =
                    absl::StrSplit(extraArgs, ',', absl::SkipWhitespace());
                break;
            case SplitMessageText::None:
                // No-op, considering one argument.
                arguments.emplace_back(extraArgs);
                break;
            case SplitMessageText::ByNewline:
                arguments =
                    absl::StrSplit(extraArgs, '\n', absl::SkipWhitespace());
                break;
        }
        for (auto& x : arguments) {
            absl::StripAsciiWhitespace(&x);
        }
    }
}

}  // namespace tgbot_api
