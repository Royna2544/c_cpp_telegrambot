#pragma once

#include <cstddef>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "LLMBackend.hpp"

namespace llm::tool_safety {

inline constexpr std::size_t kMaxToolCallsPerResponse = 4;
inline constexpr std::size_t kMaxToolCallsPerTurn = 8;
inline constexpr std::size_t kMaxToolResultBytes = 16 * 1024;

// Checks a complete provider tool-call batch before any call in it runs. This
// avoids partially executing an oversized or malformed parallel batch.
class ToolCallBudget {
   public:
    [[nodiscard]] std::optional<std::string> acceptBatch(
        std::span<const std::string> callIds) {
        if (callIds.size() > kMaxToolCallsPerResponse) {
            return "the model requested more than 4 tools in one response";
        }
        if (acceptedCalls_ + callIds.size() > kMaxToolCallsPerTurn) {
            return "the model requested more than 8 tools in one turn";
        }

        std::unordered_set<std::string> batchIds;
        for (const auto& id : callIds) {
            if (id.empty()) {
                return "a tool call had an empty id";
            }
            if (seenCallIds_.contains(id) || !batchIds.emplace(id).second) {
                return "the model repeated tool call id " + id;
            }
        }

        seenCallIds_.insert(batchIds.begin(), batchIds.end());
        acceptedCalls_ += callIds.size();
        return std::nullopt;
    }

    [[nodiscard]] std::size_t acceptedCalls() const noexcept {
        return acceptedCalls_;
    }

   private:
    std::size_t acceptedCalls_{};
    std::unordered_set<std::string> seenCallIds_;
};

inline std::string boundedToolResult(std::string result) {
    if (result.size() <= kMaxToolResultBytes) {
        return result;
    }
    constexpr std::string_view marker = "\n[tool result truncated]";
    std::size_t end = kMaxToolResultBytes - marker.size();
    while (end > 0 &&
           (static_cast<unsigned char>(result[end]) & 0xC0U) == 0x80U) {
        --end;
    }
    result.resize(end);
    result += marker;
    return result;
}

inline bool isConsequential(std::string_view toolName) {
    return toolName == "send_message" || toolName == "save_chat_info" ||
           toolName == "kernelbuild" || toolName == "rombuild";
}

inline bool requiresExplicitApproval(std::string_view toolName) {
    // Builder tools only stage their own final Telegram review. The other
    // write tools have no downstream confirmation and must be approved here.
    return toolName == "send_message" || toolName == "save_chat_info";
}

// nlohmann's default object representation orders keys, so dump() gives one
// stable representation for semantically identical object key orderings.
inline std::string canonicalAction(std::string_view toolName,
                                   const nlohmann::json& input) {
    return std::string(toolName) + "\n" + input.dump();
}

// Per-/ask-turn execution boundary. It enforces the router's exact allowlist,
// binds human approval to the precise tool+arguments, consumes approvals once,
// and permits at most one consequential action attempt in the turn.
class ToolExecutionPolicy {
   public:
    explicit ToolExecutionPolicy(std::span<const std::string> allowedTools)
        : allowedTools_(allowedTools.begin(), allowedTools.end()) {}

    explicit ToolExecutionPolicy(const std::vector<Tool>& allowedTools) {
        for (const auto& tool : allowedTools) {
            allowedTools_.emplace(tool.name);
        }
    }

    [[nodiscard]] bool recordApproval(std::string_view toolName,
                                      const nlohmann::json& input) {
        const std::lock_guard lock(mutex_);
        if (!allowedTools_.contains(std::string(toolName)) ||
            !requiresExplicitApproval(toolName) ||
            consequentialActionAttempted_) {
            return false;
        }
        approvedAction_ = canonicalAction(toolName, input);
        return true;
    }

    std::string execute(const std::string& toolName,
                        const nlohmann::json& input,
                        const ToolExecutor& delegate, bool& isError) {
        {
            const std::lock_guard lock(mutex_);
            if (!allowedTools_.contains(toolName)) {
                isError = true;
                return "Tool call rejected: " + toolName +
                       " was not exposed for this routed request.";
            }

            if (isConsequential(toolName)) {
                if (consequentialActionAttempted_) {
                    isError = true;
                    return "Tool call rejected: only one consequential action "
                           "is allowed per turn.";
                }
                if (requiresExplicitApproval(toolName)) {
                    const auto requested = canonicalAction(toolName, input);
                    if (!approvedAction_ || *approvedAction_ != requested) {
                        isError = true;
                        return "Tool call rejected: this exact action has not "
                               "been approved by the initiating admin.";
                    }
                    approvedAction_.reset();
                }
                // Count attempts, including downstream validation/API errors,
                // so a model cannot probe or retry writes repeatedly.
                consequentialActionAttempted_ = true;
            }
        }

        return delegate(toolName, input, isError);
    }

   private:
    std::mutex mutex_;
    std::unordered_set<std::string> allowedTools_;
    std::optional<std::string> approvedAction_;
    bool consequentialActionAttempted_{};
};

}  // namespace llm::tool_safety
