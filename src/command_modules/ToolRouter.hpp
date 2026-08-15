#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace llm::tool_router {

enum class Domain {
    Chat,
    KernelBuild,
    RomBuild,
    Build,
    Telegram,
    ChatRegistry,
    Confirmation,
    // Not a classifier label: the fallback used when routing could not be
    // decided (classifier unreachable or unparsable output). Exposes every
    // tool and lets the model pick, rather than silently degrading a genuine
    // tool request into a plain chat answer.
    All,
};

struct PendingBuilds {
    bool kernel = false;
    bool rom = false;
};

inline constexpr std::string_view kClassifierPrompt =
    R"(You are a tool capability router. Classify the user's request and output
exactly one lowercase label with no explanation:

kernel_build - create or update a Linux kernel build plan
rom_build - create or update an Android ROM or recovery build plan
build - explicitly wants a kernel/ROM build, but the type is unclear
telegram - send or direct-message someone on Telegram
chat_registry - save or look up a Telegram chat/user name or numeric ID
confirmation - ask the invoking admin a yes/no/cancel question
chat - none of the tool capabilities above are required

Questions merely discussing kernels, Android, Telegram, IDs, or builds are
chat. Eureka, GrassKernel, and Grand Kernel are kernel projects, never ROM
projects. LineageOS, DerpFest, crDroid, YAAP, Evolution X, and TWRP are ROM or
recovery projects. Select a tool label only when the user wants the
corresponding action.)";

inline std::string_view name(Domain domain) {
    switch (domain) {
        case Domain::Chat:
            return "chat";
        case Domain::KernelBuild:
            return "kernel_build";
        case Domain::RomBuild:
            return "rom_build";
        case Domain::Build:
            return "build";
        case Domain::Telegram:
            return "telegram";
        case Domain::ChatRegistry:
            return "chat_registry";
        case Domain::Confirmation:
            return "confirmation";
        case Domain::All:
            return "all";
    }
    return "chat";
}

namespace detail {

inline std::string normalized(std::string_view value) {
    std::string out;
    out.reserve(value.size() + 2);
    out.push_back(' ');
    bool previousSpace = true;
    for (const unsigned char ch : value) {
        if (std::isalnum(ch) || ch == '_' || ch == '-') {
            out.push_back(static_cast<char>(std::tolower(ch)));
            previousSpace = false;
        } else if (!previousSpace) {
            out.push_back(' ');
            previousSpace = true;
        }
    }
    if (!previousSpace) {
        out.push_back(' ');
    }
    return out;
}

inline bool containsTerm(std::string_view normalizedValue,
                         std::string_view term) {
    const std::string needle = " " + std::string(term) + " ";
    return normalizedValue.find(needle) != std::string_view::npos;
}

template <typename... Terms>
inline bool containsAny(std::string_view value, Terms... terms) {
    return (containsTerm(value, terms) || ...);
}

}  // namespace detail

// Handles high-confidence and stateful cases without spending a classifier
// inference. std::nullopt means the compact classifier should decide.
inline std::optional<Domain> deterministicRoute(std::string_view query,
                                                PendingBuilds pending = {}) {
    const auto value = detail::normalized(query);
    const bool exactResponse = value.starts_with(" say exactly ") ||
                               value.starts_with(" please say exactly ");
    if (exactResponse) {
        // A literal response instruction needs no capability. Keep mixed
        // requests on the classifier path, though: quoted-looking text can
        // still contain a real action after the requested phrase.
        const bool capabilityCue =
            detail::containsAny(
                value, "build", "compile", "rebuild", "kernel", "eureka",
                "grasskernel", "rom", "android", "recovery", "twrp",
                "lineageos", "derpfest", "derpfestnew", "crdroid", "yaap",
                "evolution-x", "send", "message", "telegram", "dm",
                "direct-message", "forward", "tell", "reply", "contact",
                "lookup", "retrieve", "save", "register", "remember", "chat",
                "id", "alias", "confirm", "confirmation") ||
            value.find(" grand kernel ") != std::string::npos ||
            value.find(" look up ") != std::string::npos ||
            value.find(" ask me ") != std::string::npos ||
            value.find(" ask the admin ") != std::string::npos ||
            value.find(" and ask ") != std::string::npos;
        if (!capabilityCue) {
            return Domain::Chat;
        }
        return std::nullopt;
    }
    const bool discussionPrefix =
        value.starts_with(" how ") || value.starts_with(" what ") ||
        value.starts_with(" why ") || value.starts_with(" when ") ||
        value.starts_with(" where ") || value.starts_with(" who ") ||
        value.starts_with(" explain ") || value.starts_with(" describe ") ||
        value.starts_with(" compare ") || value.starts_with(" tell me about ");
    const bool discussion =
        discussionPrefix ||
        detail::containsAny(value, "explain", "explanation", "describe",
                            "description", "compare", "comparison",
                            "difference", "meaning") ||
        value.find(" tell me about ") != std::string::npos ||
        query.find('?') != std::string_view::npos;
    const bool buildAction =
        detail::containsAny(value, "build", "compile", "prepare", "rebuild",
                            "make", "want", "need", "run", "start", "launch");
    const bool kernel =
        detail::containsAny(value, "kernel", "eureka", "grasskernel") ||
        value.find(" grand kernel ") != std::string::npos;
    const bool rom = detail::containsAny(
        value, "rom", "android", "recovery", "twrp", "lineageos", "derpfest",
        "derpfestnew", "crdroid", "yaap", "evolution-x");

    if (!discussion && buildAction && kernel && !rom) {
        return Domain::KernelBuild;
    }
    if (!discussion && buildAction && rom && !kernel) {
        return Domain::RomBuild;
    }
    if (!discussion && buildAction && kernel && rom) {
        return Domain::Build;
    }

    // Explicit new build intent above overrides an unrelated pending plan.
    // Only field-like or genuinely short follow-ups inherit pending state;
    // an unrelated request must still reach the capability classifier.
    if (pending.kernel || pending.rom) {
        const bool anotherCapability = detail::containsAny(
            value, "send", "message", "telegram", "dm", "email", "save",
            "remember", "lookup", "contact");
        const bool buildField = detail::containsAny(
            value, "device", "branch", "repo", "repository", "compiler",
            "defconfig", "variant", "target", "clean", "clang", "gcc",
            "toolchain", "jobs", "userdebug", "eng");
        const auto wordCount = static_cast<std::size_t>(
            std::count(value.begin(), value.end(), ' ') - 1);
        const bool shortReply = wordCount <= 12 && query.size() <= 160;
        if (!discussion && !anotherCapability && (shortReply || buildField)) {
            if (pending.kernel && pending.rom) {
                return Domain::Build;
            }
            return pending.kernel ? Domain::KernelBuild : Domain::RomBuild;
        }
    }
    return std::nullopt;
}

inline std::optional<Domain> parseClassifierResult(std::string_view result) {
    const auto value = detail::normalized(result);
    std::optional<Domain> parsed;
    for (std::size_t start = 0; start < value.size();) {
        while (start < value.size() && value[start] == ' ') {
            ++start;
        }
        const auto end = value.find(' ', start);
        const auto token = value.substr(start, end - start);
        std::optional<Domain> candidate;
        if (token == "kernel_build") {
            candidate = Domain::KernelBuild;
        } else if (token == "rom_build") {
            candidate = Domain::RomBuild;
        } else if (token == "build") {
            candidate = Domain::Build;
        } else if (token == "telegram") {
            candidate = Domain::Telegram;
        } else if (token == "chat_registry") {
            candidate = Domain::ChatRegistry;
        } else if (token == "confirmation") {
            candidate = Domain::Confirmation;
        } else if (token == "chat") {
            candidate = Domain::Chat;
        }
        if (candidate) {
            if (parsed && parsed != candidate) {
                return std::nullopt;
            }
            parsed = candidate;
        }
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return parsed;
}

// A failed classification is not evidence that no tool is wanted, so it must
// not collapse to Domain::Chat - that would answer a real build/telegram
// request as plain chat with no way for the model to act and no error shown.
// Both failure paths (no response, unparsable response) fall back to Domain::
// All. A classifier that explicitly answers "chat" is still honoured.
template <typename Classify>
inline Domain selectDomain(std::string_view query, PendingBuilds pending,
                           Classify&& classify) {
    if (const auto deterministic = deterministicRoute(query, pending)) {
        return *deterministic;
    }
    if (const auto result =
            std::forward<Classify>(classify)(kClassifierPrompt, query)) {
        return parseClassifierResult(*result).value_or(Domain::All);
    }
    return Domain::All;
}

// Every domain whose tools act without a confirmation step of their own also
// gets "ask", so the model can confirm and then act within one turn. The
// builder domains deliberately do not: kernelbuild/rombuild stage a final
// Telegram review of their own, and the build system prompt tells the model
// not to ask questions it could answer by calling the builder tool.
inline std::span<const std::string_view> toolNamesForDomain(Domain domain) {
    static constexpr std::array<std::string_view, 1> kKernelBuild{
        "kernelbuild"};
    static constexpr std::array<std::string_view, 1> kRomBuild{"rombuild"};
    static constexpr std::array<std::string_view, 2> kBuild{"kernelbuild",
                                                            "rombuild"};
    static constexpr std::array<std::string_view, 4> kTelegram{
        "send_message", "get_chat_id", "get_chat_name", "ask"};
    static constexpr std::array<std::string_view, 4> kChatRegistry{
        "get_chat_id", "get_chat_name", "save_chat_info", "ask"};
    static constexpr std::array<std::string_view, 1> kConfirmation{"ask"};
    static constexpr std::array<std::string_view, 7> kAll{
        "kernelbuild",   "rombuild",       "send_message", "get_chat_id",
        "get_chat_name", "save_chat_info", "ask"};

    switch (domain) {
        case Domain::Chat:
            return {};
        case Domain::KernelBuild:
            return kKernelBuild;
        case Domain::RomBuild:
            return kRomBuild;
        case Domain::Build:
            return kBuild;
        case Domain::Telegram:
            return kTelegram;
        case Domain::ChatRegistry:
            return kChatRegistry;
        case Domain::Confirmation:
            return kConfirmation;
        case Domain::All:
            return kAll;
    }
    return {};
}

}  // namespace llm::tool_router
