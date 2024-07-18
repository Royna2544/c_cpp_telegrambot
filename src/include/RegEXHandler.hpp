#pragma once

#include <TgBotPPImplExports.h>
#include <absl/status/status.h>
#include <memory>
#include <optional>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include "CStringLifetime.h"
#include "TgBotWrapper.hpp"
#include "initcalls/Initcall.hpp"

using std::regex_constants::syntax_option_type;
using TgBot::Bot;
using TgBot::Message;

template <typename T>
class OptionalWrapper {
    std::optional<T> val;

   public:
    OptionalWrapper(const std::optional<T> _val) : val(_val) {}
    OptionalWrapper() : val(std::nullopt) {}

    bool has_value() const { return val.has_value(); }
    T value() const { return val.value(); }
    T operator*() const { return value(); }
    operator bool() const { return has_value(); }
    OptionalWrapper<T> operator|=(OptionalWrapper<T>&& other) {
        if (this != &other) {
            if (other.has_value() && !has_value()) {
                val = std::move(other.val);
            }
        }
        return *this;
    }
};

struct TgBotPPImpl_API RegexHandlerBase {
    struct Interface {
        virtual ~Interface() = default;
        // Called when regex processing is complete. Error or success
        virtual void onError(const absl::Status& status) = 0;
        virtual void onSuccess(const std::string& result) = 0;
    };

    explicit RegexHandlerBase(std::shared_ptr<Interface> interface) : _interface(std::move(interface)) {}
    virtual ~RegexHandlerBase() = default;

    // Available functions
    enum class Command {
        RegexReplace,
        RegexDelete,
    };

    // Context for making this class reusable against changed params
    struct Context {
        std::string regexCommand;  // e.g. s/s\d+/ssss/g
        std::string text;
    };

    // Process regex pattern in the given context.
    // If regex processing fails, it calls onComplete with appropriate status.
    // If regex processing succeeds, it calls onComplete with a success status.
    void process();

    void setContext(Context&& context) { _context = std::move(context); }

   private:
    std::optional<Context> _context;

    // Tries to match a regex syntax, (if it is sed delete command or replace
    // command), and returns a vector of substrings sperated by '/'
    std::vector<std::string> tryParseCommand(const std::regex& regexMatcher);

    // Tries to construct regex given pattern and flags
    std::optional<std::regex> constructRegex(const std::string& pattern,
                                             const syntax_option_type flags);

   protected:
    // TODO: For testing purposes
    std::shared_ptr<Interface> _interface;
    OptionalWrapper<std::string> doRegexReplaceCommand();
    OptionalWrapper<std::string> doRegexDeleteCommand();
};

struct TgBotPPImpl_API RegexHandler : public InitCall {
    RegexHandler() = default;
    ~RegexHandler() = default;

    void doInitCall() override;
    const CStringLifetime getInitCallName() const override {
        return TgBotWrapper::getInitCallNameForClient("RegexHandler");
    }
};
