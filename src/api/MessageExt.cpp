#include <AbslLogCompat.hpp>
#include <absl/strings/ascii.h>
#include <absl/strings/str_split.h>
#include <absl/strings/strip.h>

#include <api/MessageExt.hpp>
#include <memory>

MessageExt::MessageExt(Message::Ptr message, SplitMessageText how)
    : _message(std::move(message)) {
    if (!_message) {
        return;
    }
    _replyMessage = std::make_shared<MessageExt>(_message->replyToMessage);

    // Empty message won't need parsing
    if (!_message->text) {
        return;
    }

    // Initially, _extra_args is full text
    _extra_args = _message->text.value();

    // Try to find botcommand entity
    const auto botCommandEnt =
        std::ranges::find_if(_message->entities, [](const auto& entity) {
            return entity->type == TgBot::MessageEntity::Type::BotCommand &&
                   entity->offset == 0;
        });

    // I believe entity must be sent here.
    if (botCommandEnt != _message->entities.end() &&
        _message->text->front() == '/') {
        const auto entry = *botCommandEnt;
        // Grab /start@username
        _extra_args = _message->text->substr(entry->length);
        absl::StripLeadingAsciiWhitespace(&_extra_args);
        command.emplace();
        std::pair<std::string, std::string> kCommandSplit =
            absl::StrSplit(_message->text->substr(1, entry->length), "@");
        command->name = kCommandSplit.first;
        command->target = kCommandSplit.second;
        absl::StripTrailingAsciiWhitespace(&command->target);
    }

    if (_extra_args.size() != 0) {
        switch (how) {
            case SplitMessageText::ByWhitespace:
                _arguments =
                    absl::StrSplit(_extra_args, ' ', absl::SkipWhitespace());
                break;
            case SplitMessageText::ByComma:
                _arguments =
                    absl::StrSplit(_extra_args, ',', absl::SkipWhitespace());
                break;
            case SplitMessageText::None:
                // No-op, considering one argument.
                _arguments.emplace_back(_extra_args);
                break;
            case SplitMessageText::ByNewline:
                _arguments =
                    absl::StrSplit(_extra_args, '\n', absl::SkipWhitespace());
                break;
        }
        for (auto& x : _arguments) {
            absl::StripAsciiWhitespace(&x);
        }
    }
}