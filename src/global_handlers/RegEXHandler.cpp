#include <absl/log/log.h>
#include <absl/status/status.h>
#include <absl/strings/ascii.h>
#include <fmt/format.h>

#include <cctype>
#include <expected_cpp20>
#include <global_handlers/RegEXHandler.hpp>
#include <iomanip>
#include <optional>
#include <regex>
#include <sstream>
#include <string>

#include "TinyStatus.hpp"

using std::regex_constants::ECMAScript;
using std::regex_constants::format_first_only;
using std::regex_constants::format_sed;
using std::regex_constants::icase;
using std::regex_constants::match_not_null;

[[nodiscard]] RegexCommand::Result RegexCommand::process(
    const std::string& source, const std::string& regexCommand) const {
    std::smatch match;
    if (std::regex_match(regexCommand, match, command_regex())) {
        try {
            return process(source, match);
        } catch (const std::regex_error& e) {
            LOG(ERROR) << "Invalid regex: " << e.what();
            return compat::unexpected{Error::InvalidRegex};
        }
    }
    return compat::unexpected{Error::None};
}

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
        // No registered regex commands, just return.
        return;
    }
    for (const auto& handler : _handlers) {
        RegexCommand::Result ret = handler->process(source, regexCommand);
        if (ret.has_value()) {
            callback->onSuccess(*ret);
        } else {
            switch (ret.error()) {
                case RegexCommand::Error::InvalidRegexOption:
                    callback->onError(tinystatus::TinyStatus(
                        tinystatus::Status::kInvalidArgument,
                        "Invalid regex option in command: " + regexCommand));
                    return;
                case RegexCommand::Error::GlobalFlagAndMatchIndexInvalid:
                    callback->onError(tinystatus::TinyStatus(
                        tinystatus::Status::kInvalidArgument,
                        "Global flag and match index are not compatible: " +
                            regexCommand));
                    return;
                case RegexCommand::Error::InvalidRegexMatchIndex:
                    callback->onError(tinystatus::TinyStatus(
                        tinystatus::Status::kInvalidArgument,
                        "Invalid regex match index: " + regexCommand));
                    return;
                case RegexCommand::Error::InvalidRegex:
                    callback->onError(tinystatus::TinyStatus(
                        tinystatus::Status::kInvalidArgument,
                        "Invalid regex: " + regexCommand));
                    return;
                case RegexCommand::Error::None:
                    // No error, just continue to the next handler
                    break;
                default:
                    callback->onError(tinystatus::TinyStatus(
                        tinystatus::Status::kInternalError,
                        "An unexpected error occurred while processing "
                        "regex command: " +
                            regexCommand));
                    return;
            }
        }
    }
}

// clang-format off
/*
 * =========================================================================================
 * REGEX COMMAND REFERENCE
 * =========================================================================================
 *
 * 1. Command:     Replace
 *    Syntax:      s/<target>/<replacement>/[flags]
 *    Description: Replaces occurrences of 'target' with 'replacement'.
 *    Flags: 'g' (global), 'i' (case-insensitive), or integer (specific index).
 *    Source:      sed
 *    Examples:    s/cat/dog/g       (Replace all occurrences of 'cat' with 'dog')
 *    s/foo/bar/2       (Replace only the 2nd occurrence of 'foo' with 'bar')
 *
 * 2. Command:     Delete
 *    Syntax:      /<pattern>/d
 *    Description: Deletes entire lines from the source that contain a match for
 *                 'pattern'. 
 *    Source:      sed 
 *    Examples:    /^#/d             (Delete lines starting with '#') 
 *                 /debug/d          (Delete lines containing 'debug')
 *
 * 3. Command:     Print
 *    Syntax:      /<pattern>/p
 *    Description: Filters the source to output ONLY the lines that match 'pattern'.
 *    Source:      sed, grep
 *    Examples:    /ERROR/p          (Output only lines containing 'ERROR')
 *                 /user_\d+/p       (Output only lines containing 'user_123' format)
 *
 * 4. Command:     Count
 *    Syntax:      /<pattern>/c
 *    Description: Returns a single number representing the total matches found in
 *                 the source. 
 *    Source:      grep -c
 *    Examples:    /failed/c         (Returns count of 'failed', e.g., "5")
 *                 /./c              (Returns count of all characters)
 *
 * 5. Command:     ToUpper
 *    Syntax:      u/<pattern>/
 *    Description: Transforms the text matching 'pattern' to uppercase.
 *    Source:      vim, awk
 *    Examples:    u/id_[a-z]+/      (Converts 'id_admin' -> 'ID_ADMIN')
 *                 u/urgent/         (Converts 'urgent' -> 'URGENT')
 *
 * =========================================================================================
 */
// clang-format on

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
                } else if (absl::ascii_isdigit(option)) {
                    int value = 0;
                    while (i < options.length() &&
                           absl::ascii_isdigit(options[i])) {
                        value = (value * 10) + (options[i] - '0');
                        ++i;
                    }
                    DLOG(INFO) << "Replace Index Value: " << value;
                    replaceIndex = value;
                } else {
                    LOG(ERROR) << "Invalid regex option: " << option;
                    return compat::unexpected(Error::InvalidRegexOption);
                }
            }

            // Error if both global and specific index are set
            if ((kRegexMatchFlags & format_first_only) == 0 && replaceIndex) {
                return compat::unexpected(
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
            std::ostringstream result;
            // Track the last position
            std::string::const_iterator last_pos = source.begin();

            bool replaced = false;

            for (auto it = match; it != match_end; ++it) {
                result << std::string(
                    last_pos, it->prefix().second);  // Append unmatched part
                if (++count == *replaceIndex) {
                    result << replacement;  // Add the replacement
                    replaced = true;
                } else {
                    result << it->str();  // No replacement, keep the match
                }
                last_pos = it->suffix().first;  // Update the last position
            }

            if (!replaced) {
                LOG(INFO) << "Found " << count
                          << " matches, been told to replace " << *replaceIndex;
                return compat::unexpected(Error::InvalidRegexMatchIndex);
            }
            // Append the remainder
            result << std::string(last_pos, source.end());

            return result.str();
        }

        DLOG(INFO) << fmt::format(
            "Replace with RegEX: '{}', target: '{}', Opt: global={} icase={}",
            target, replacement, (kRegexMatchFlags & format_first_only) == 0,
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
        std::ostringstream result;
        std::istringstream stream(source);
        std::string line;
        while (std::getline(stream, line)) {
            if (!std::regex_search(line, reg)) {
                result << line << '\n';  // Append non-matching lines
            }
        }
        return result.str();
    }
};

struct PrintCommand : public RegexCommand {
    [[nodiscard]] std::regex command_regex() const override {
        // Matches /pattern/p
        static std::regex regex(R"(^\/((?:[^\/\\]|\\.)+)\/p$)");
        return regex;
    }

    [[nodiscard]] std::string_view description() const override {
        return "Print only lines matching pattern (grep style)";
    }

    [[nodiscard]] Result process(const std::string& source,
                                 const std::smatch command) const override {
        const auto& pattern = command[1].str();
        std::regex reg(pattern);

        std::ostringstream result;
        std::istringstream stream(source);
        std::string line;

        while (std::getline(stream, line)) {
            // Only keep lines that MATCH the pattern
            if (std::regex_search(line, reg)) {
                result << line << '\n';
            }
        }
        return result.str();
    }
};

struct CountCommand : public RegexCommand {
    [[nodiscard]] std::regex command_regex() const override {
        // Matches /pattern/c
        static std::regex regex(R"(^\/((?:[^\/\\]|\\.)+)\/c$)");
        return regex;
    }

    [[nodiscard]] std::string_view description() const override {
        return "Count occurrences of pattern";
    }

    [[nodiscard]] Result process(const std::string& source,
                                 const std::smatch command) const override {
        const auto& pattern = command[1].str();
        std::regex reg(pattern);

        // Use iterator distance to count matches efficiently
        auto words_begin =
            std::sregex_iterator(source.begin(), source.end(), reg);
        auto words_end = std::sregex_iterator();

        auto count = std::distance(words_begin, words_end);

        // Return the count as the "result" string
        return std::to_string(count);
    }
};
struct ToUpperCommand : public RegexCommand {
    [[nodiscard]] std::regex command_regex() const override {
        // Matches u/pattern/
        static std::regex regex(R"(^u\/((?:[^\/\\]|\\.)+)\/$)");
        return regex;
    }

    [[nodiscard]] std::string_view description() const override {
        return "Convert matches to Uppercase";
    }

    [[nodiscard]] Result process(const std::string& source,
                                 const std::smatch command) const override {
        const auto& pattern = command[1].str();
        std::regex reg(pattern);

        std::ostringstream result;
        auto match_iter =
            std::sregex_iterator(source.begin(), source.end(), reg);
        auto match_end = std::sregex_iterator();

        std::string::const_iterator last_pos = source.begin();

        for (auto it = match_iter; it != match_end; ++it) {
            // 1. Append text BEFORE the match
            result << std::string(last_pos, it->prefix().second);

            // 2. Transform the matched text to upper case
            std::string match_str = it->str();
            for (char& c : match_str) {
                c = absl::ascii_toupper(c);  // Using absl as per your includes
            }
            result << match_str;

            // 3. Update position
            last_pos = it->suffix().first;
        }

        // 4. Append remaining text
        result << std::string(last_pos, source.end());

        return result.str();
    }
};

RegexHandler::RegexHandler() {
    // Register the supported regex commands
    registerCommand(std::make_unique<ReplaceCommand>());
    registerCommand(std::make_unique<DeleteCommand>());
    registerCommand(std::make_unique<PrintCommand>());
    registerCommand(std::make_unique<CountCommand>());
    registerCommand(std::make_unique<ToUpperCommand>());
}