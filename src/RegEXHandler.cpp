#include <absl/log/log.h>
#include <absl/status/status.h>
#include <absl/strings/ascii.h>

#include <RegEXHandler.hpp>
#include <ios>
#include <optional>
#include <regex>
#include <string>

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

std::vector<std::string> RegexHandlerBase::tryParseCommand(
    const std::regex& regexMatcher) {
    if (std::regex_match(_context->regexCommand, regexMatcher)) {
        return StringTools::split(_context->regexCommand, '/');
    }
    return {};
}

std::optional<std::regex> RegexHandlerBase::constructRegex(
    const std::string& pattern, const syntax_option_type flags) {
    try {
        return std::regex(pattern, flags);
    } catch (const std::regex_error& e) {
        _interface->onError(absl::InvalidArgumentError(e.what()));
        return std::nullopt;
    }
}

OptionalWrapper<std::string> RegexHandlerBase::doRegexReplaceCommand() {
    std::regex src;
    std::string dest;
    bool global = false;  // g flag in sed
    auto kRegexFlags = ECMAScript;
    auto kRegexMatchFlags = format_sed | match_not_null;
    auto args = tryParseCommand(kSedReplaceCommandRegex);

    // s/aaaa/bbbb/g (split to 4 strings)
    // s/aaaa/bbbb   (split to 3 strings)
    // Else invalid
    if (auto len = args.size(); len == 3 || len == 4) {
        if (len == 4) {
            const auto& opt = args[3];
            // Due to the above regex match, it should be either none, i, g, ig,
            // gi
            global = opt.find('g') != std::string::npos;
            if (opt.find('i') != std::string::npos) {
                kRegexFlags |= icase;
            }
        }
        if (auto regex = constructRegex(args[1], kRegexFlags);
            regex.has_value()) {
            src = regex.value();
            dest = args[2];

            if (!global) {
                kRegexMatchFlags |= format_first_only;
            }

            DLOG(INFO) << "src: '" << args[1] << "' dest: '" << args[2]
                       << std::boolalpha << "' global: " << global
                       << " icase: " << (kRegexFlags & icase);
            try {
                return {std::regex_replace(_context->text, src, dest,
                                           kRegexMatchFlags)};
            } catch (const std::regex_error& e) {
                _interface->onError(absl::InvalidArgumentError(e.what()));
            }
        }
    }

    return {std::nullopt};
}

OptionalWrapper<std::string> RegexHandlerBase::doRegexDeleteCommand() {
    auto args = tryParseCommand(kSedDeleteCommandRegex);
    // /aaaa/d
    if (args.size() == 3) {
        if (auto regex = constructRegex(args[1], ECMAScript);
            regex.has_value()) {
            std::stringstream kInStream(_context->text);
            std::stringstream kOutStream;
            std::string line;
            std::string out;
            DLOG(INFO) << "regexstr: '" << args[1] << "'";
            while (std::getline(kInStream, line)) {
                if (!std::regex_search(line, regex.value(),
                                       format_sed | match_not_null)) {
                    kOutStream << line << std::endl;
                }
            }
            out = kOutStream.str();
            out = absl::StripAsciiWhitespace(out);
            return {out};
        }
    }
    return {std::nullopt};
}

void RegexHandlerBase::process() {
    OptionalWrapper<std::string> ret;

    if (!_context.has_value()) {
        _interface->onError(absl::InvalidArgumentError("No context provided"));
        return;
    }
    ret |= doRegexReplaceCommand();
    ret |= doRegexDeleteCommand();
    if (ret) {
        _interface->onSuccess(*ret);
    }
    _context.reset();
}

DECLARE_CLASS_INST(RegexHandler);