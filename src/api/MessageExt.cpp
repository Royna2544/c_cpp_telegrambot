#include <absl/log/log.h>
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
    // Initially, _extra_args is full text
    _extra_args = _message->text;
    // Try to find botcommand entity
    const auto botCommandEnt =
        std::ranges::find_if(_message->entities, [](const auto& entity) {
            return entity->type == TgBot::MessageEntity::Type::BotCommand &&
                   entity->offset == 0;
        });

    // I believe entity must be sent here.
    if (botCommandEnt != _message->entities.end()) {
        const auto entry = *botCommandEnt;
        // Grab /start@username
        _extra_args = absl::StripAsciiWhitespace(
            _message->text.substr(entry->offset + entry->length));
        auto command_string = _message->text.substr(0, entry->length);
        command_string = absl::StripPrefix(command_string, "/");
        command.emplace();
        const auto at_pos = command_string.find('@');
        if (at_pos != std::string::npos) {
            command->name = command_string.substr(0, at_pos);
            command->target = command_string.substr(at_pos + 1);
            LOG_IF(WARNING, command->target.empty() || command->name.empty())
                << "Parsing logic error, target or name is empty";
        } else {
            command->name = command_string;
        }
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
                break;
        }
    }
}