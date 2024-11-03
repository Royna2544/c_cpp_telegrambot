#include <absl/log/log.h>
#include <absl/status/status.h>
#include <fmt/format.h>

#include <cctype>
#include <expected_cpp20>
#include <global_handlers/RegEXHandler.hpp>
#include <ios>
#include <optional>
#include <regex>
#include <string>

using std::regex_constants::ECMAScript;
using std::regex_constants::format_first_only;
using std::regex_constants::format_sed;
using std::regex_constants::icase;
using std::regex_constants::match_not_null;

namespace Vstd {
bool isdigit(const char c) { return std::isdigit(c) != 0; }
}  // namespace Vstd

[[nodiscard]] RegexCommand::Result RegexCommand::process(
    const std::string& source, const std::string& regexCommand) const {
    std::smatch match;
    if (std::regex_match(regexCommand, match, command_regex())) {
        try {
            return process(source, match);
        } catch (const std::regex_error& e) {
            LOG(ERROR) << "Invalid regex: " << e.what();
            return std_cpp20::unexpected{Error::InvalidRegex};
        }
    }
    return std_cpp20::unexpected{Error::None};
}

struct ReplaceCommand : public RegexCommand {
    [[nodiscard]] std::regex command_regex() const override {
        static std::regex regex(
            R"(^s\/((?:[^\/\\]|\\.)+)\/((?:[^\/\\]|\\.)+)(\/)?(([gi\d]+))?$)");
        return regex;
    }

    [[nodiscard]] std::string_view description() const override {
        return "Replace text (sed style)";
    }

    [[nodiscard]] RegexCommand::Result process(
        const std::string& source, const std::smatch command) const override {
        bool hasOptions = command.size() == 6;
        const auto& target = command[1].str();
        const auto& replacement = command[2].str();

        // Assume 'g' flag is set by default.
        auto kRegexMatchFlags = format_sed | match_not_null | format_first_only;
        auto kRegexFlags = ECMAScript;
        std::optional<int> replaceIndex;

        if (hasOptions) {
            const auto& options = command[5].str();
            for (size_t i = 0; i < options.length();) {
                char option = options[i];
                if (option == 'g') {
                    // Global replacement
                    kRegexMatchFlags &= ~format_first_only;
                    ++i;
                } else if (option == 'i') {
                    // Case-insensitive
                    kRegexFlags |= icase;
                    ++i;
                } else if (Vstd::isdigit(option)) {
                    int value = 0;
                    while (i < options.length() && Vstd::isdigit(options[i])) {
                        value = value * 10 + (options[i] - '0');
                        ++i;
                    }
                    DLOG(INFO) << "Replace Index Value: " << value;
                    replaceIndex = value;
                } else {
                    LOG(ERROR) << "Invalid regex option: " << option;
                    return std_cpp20::unexpected(Error::InvalidRegexOption);
                }
            }

            // Error if both global and specific index are set
            if ((kRegexMatchFlags & format_first_only) == 0 && replaceIndex) {
                return std_cpp20::unexpected(
                    Error::GlobalFlagAndMatchIndexInvalid);
            }
        }

        if (replaceIndex) {
            // Perform replacement at a specific match index
            std::regex reg(target, kRegexFlags);
            auto match =
                std::sregex_iterator(source.begin(), source.end(), reg);
            auto match_end = std::sregex_iterator();

            int count = 0;
            std::string result;
            // Track the last position
            std::string::const_iterator last_pos = source.begin();

            bool replaced = false;

            for (auto it = match; it != match_end; ++it) {
                result += std::string(
                    last_pos, it->prefix().second);  // Append unmatched part
                if (++count == *replaceIndex) {
                    result += replacement;  // Add the replacement
                    replaced = true;
                } else {
                    result += it->str();  // No replacement, keep the match
                }
                last_pos = it->suffix().first;  // Update the last position
            }

            if (!replaced) {
                LOG(INFO) << "Found " << count
                          << " matches, been told to replace " << *replaceIndex;
                return std_cpp20::unexpected(Error::InvalidRegexMatchIndex);
            }
            // Append the remainder
            result += std::string(last_pos, source.end());

            return result;
        }

        DLOG(INFO) << fmt::format(
            "Replace with RegEX: '{}', target: '{}', Opt: global={} icase={}", target,
            replacement, (kRegexMatchFlags & format_first_only) == 0,
            (kRegexFlags & icase) == 0);

        // Global replacement or case-insensitive
        return std::regex_replace(source, std::regex(target, kRegexFlags),
                                  replacement, kRegexMatchFlags);
    }
};

struct DeleteCommand : public RegexCommand {
    [[nodiscard]] std::regex command_regex() const override {
        // Match for the delete command pattern like /pattern/d
        static std::regex regex(R"(^\/((?:[^\/\\]|\\.)+)\/d$)");
        return regex;
    }

    [[nodiscard]] std::string_view description() const override {
        return "Delete lines matching pattern (sed style)";
    }

    [[nodiscard]] Result process(const std::string& source,
                                 const std::smatch command) const override {
        const auto& pattern = command[1].str();  // Get the pattern

        std::regex reg(pattern);  // Compile the regex

        // Use regex iterator to filter lines that do not match the pattern
        std::string result;
        std::istringstream stream(source);
        std::string line;
        while (std::getline(stream, line)) {
            if (!std::regex_search(line, reg)) {
                result += line + "\n";  // Append non-matching lines
            }
        }
        return result;
    }
};

void RegexHandler::registerCommand(std::unique_ptr<RegexCommand> handler) {
    std::lock_guard<std::mutex> lock(_mutex);
    LOG(INFO) << "Registering handler: " << std::quoted(handler->description());
    // Transfer ownership to the vector
    _handlers.emplace_back(std::move(handler));
}

void RegexHandler::execute(const std::shared_ptr<Interface>& callback,
                           const std::string& source,
                           const std::string& regexCommand) {
    std::lock_guard<std::mutex> lock(_mutex);

    if (_handlers.empty()) {
        // No registered regex commands, notify the callback with an empty
        // result
        callback->onError(absl::InternalError("No registered regex commands"));
        return;
    }
    for (const auto& handler : _handlers) {
        RegexCommand::Result ret = handler->process(source, regexCommand);
        if (ret.has_value()) {
            callback->onSuccess(*ret);
        } else {
            switch (ret.error()) {
                case RegexCommand::Error::InvalidRegexOption:
                    callback->onError(absl::InvalidArgumentError(
                        "Invalid regex option in command: " + regexCommand));
                    return;
                case RegexCommand::Error::GlobalFlagAndMatchIndexInvalid:
                    callback->onError(absl::InvalidArgumentError(
                        "Global flag and match index are not compatible: " +
                        regexCommand));
                    return;
                case RegexCommand::Error::InvalidRegexMatchIndex:
                    callback->onError(absl::InvalidArgumentError(
                        "Invalid regex match index: " + regexCommand));
                    return;
                case RegexCommand::Error::InvalidRegex:
                    callback->onError(absl::InvalidArgumentError(
                        "Invalid regex: " + regexCommand));
                    return;
                case RegexCommand::Error::None:
                    // No error, just continue to the next handler
                    break;
                default:
                    callback->onError(absl::InternalError(
                        "An unexpected error occurred while processing "
                        "regex command: " +
                        regexCommand));
                    return;
            }
        }
    }
}

RegexHandler::RegexHandler() {
    // Register the supported regex commands
    registerCommand(std::make_unique<ReplaceCommand>());
    registerCommand(std::make_unique<DeleteCommand>());
}