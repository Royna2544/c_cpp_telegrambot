#pragma once

#include <absl/log/log.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <optional>
#include <string>
#include <vector>

#include "CurlUtils.hpp"
#include "LLMBackend.hpp"

// OpenAI-compatible Chat Completions backend. Also serves any server that
// implements `/v1/chat/completions` + `/v1/models` (llama-server, vLLM, etc.).
namespace llm::openai {

constexpr const char* kModelsEndpoint = "/v1/models";
constexpr const char* kChatEndpoint = "/v1/chat/completions";
constexpr int kMaxTokens = 1024;
constexpr int kMaxToolIterations = 6;

// --- Request ---
struct Message {
    std::string role;
    std::string content;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Message, role, content)

struct ChatRequest {
    std::string model;
    std::vector<Message> messages;
    int max_tokens{kMaxTokens};
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChatRequest, model, messages, max_tokens)

// --- Response (tolerant: WITH_DEFAULT ignores extra/missing fields) ---
struct RespMessage {
    std::string role;
    std::string content;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(RespMessage, role, content)

struct Choice {
    RespMessage message;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Choice, message)

struct ChatResponse {
    std::vector<Choice> choices;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ChatResponse, choices)

struct ModelEntry {
    std::string id;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ModelEntry, id)

struct ModelList {
    std::vector<ModelEntry> data;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ModelList, data)

// Standard OpenAI error envelope (also emitted by OpenAI-compatible servers
// like LM Studio): {"error": {"message", "type", "param", "code"}}. `param`
// and `code` are frequently null, hence the tolerant parsing.
struct ErrorDetail {
    std::string message;
    std::string type;
    std::string code;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ErrorDetail, message, type,
                                                code)

struct ErrorResponse {
    ErrorDetail error;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ErrorResponse, error)

// Detects an OpenAI-style error envelope and logs it (there's no way to
// surface it to the LLMBackend caller without changing the interface, and
// `/ask` is not admin-gated, so this stays server-side rather than being
// echoed back into the chat reply). Returns true if `parsed` was an error.
inline bool logIfApiError(const nlohmann::json& parsed, std::string_view endpoint) {
    if (!parsed.contains("error") || !parsed["error"].is_object()) {
        return false;
    }
    const auto err = parsed.get<ErrorResponse>().error;
    LOG(WARNING) << fmt::format(
        "OpenAI-compatible API error from {}: [{}{}] {}", endpoint,
        err.type.empty() ? "error" : err.type,
        err.code.empty() ? "" : fmt::format("/{}", err.code), err.message);
    return true;
}

class OpenAIBackend : public LLMBackend {
   public:
    OpenAIBackend(std::string url, std::string authkey)
        : url_(std::move(url)), authkey_(std::move(authkey)) {}

    std::vector<LLMModel> listModels() override {
        auto raw = CurlUtils::download_memory(url_ + kModelsEndpoint, nullptr,
                                              std::string_view{authkey_});
        if (!raw) {
            return {};
        }
        std::vector<LLMModel> out;
        try {
            const auto parsed = nlohmann::json::parse(*raw);
            if (logIfApiError(parsed, kModelsEndpoint)) {
                return {};
            }
            auto list = parsed.get<ModelList>();
            for (auto& m : list.data) {
                out.push_back({m.id, m.id});
            }
        } catch (const std::exception&) {
            return {};
        }
        return out;
    }

    std::optional<std::string> chat(const std::string& model,
                                    const std::string& systemPrompt,
                                    const std::string& userInput,
                                    std::int64_t /*chatId*/) override {
        ChatRequest req;
        req.model = model;
        req.messages = {{"system", systemPrompt}, {"user", userInput}};
        nlohmann::json payload = req;

        auto raw = CurlUtils::send_json_get_reply(
            url_ + kChatEndpoint, payload.dump(), std::string_view{authkey_});
        if (!raw) {
            return std::nullopt;
        }
        try {
            const auto parsed = nlohmann::json::parse(*raw);
            if (logIfApiError(parsed, kChatEndpoint)) {
                return std::nullopt;
            }
            auto resp = parsed.get<ChatResponse>();
            if (resp.choices.empty()) {
                return std::nullopt;
            }
            return resp.choices.front().message.content;
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    // Tool-calling variant: offers `tools` to the model and dispatches
    // `tool_calls` through `exec`, looping (raw JSON, since tool_calls/tool
    // messages don't fit the plain-string Message shape above) until the
    // model returns a final answer or kMaxToolIterations is exhausted.
    std::optional<std::string> chat(const std::string& model,
                                    const std::string& systemPrompt,
                                    const std::string& userInput,
                                    std::int64_t /*chatId*/,
                                    const std::vector<llm::Tool>& tools,
                                    llm::ToolExecutor exec) override {
        auto extractText =
            [](const nlohmann::json& msg) -> std::optional<std::string> {
            if (msg.contains("content") && msg["content"].is_string()) {
                return msg["content"].get<std::string>();
            }
            return std::nullopt;
        };

        nlohmann::json messages = nlohmann::json::array(
            {{{"role", "system"}, {"content", systemPrompt}},
             {{"role", "user"}, {"content", userInput}}});

        nlohmann::json toolsJson = nlohmann::json::array();
        for (const auto& tool : tools) {
            toolsJson.push_back(
                {{"type", "function"},
                 {"function",
                  {{"name", tool.name},
                   {"description", tool.description},
                   {"parameters", tool.inputSchema}}}});
        }

        std::optional<std::string> lastText;
        for (int iteration = 0; iteration < kMaxToolIterations; ++iteration) {
            nlohmann::json payload{{"model", model},
                                   {"messages", messages},
                                   {"max_tokens", kMaxTokens}};
            if (!toolsJson.empty()) {
                payload["tools"] = toolsJson;
            }

            auto raw = CurlUtils::send_json_get_reply(
                url_ + kChatEndpoint, payload.dump(), std::string_view{authkey_});
            if (!raw) {
                return std::nullopt;
            }

            nlohmann::json respJson;
            try {
                respJson = nlohmann::json::parse(*raw);
            } catch (const std::exception&) {
                return std::nullopt;
            }
            if (logIfApiError(respJson, kChatEndpoint)) {
                return lastText;
            }
            if (!respJson.contains("choices") || respJson["choices"].empty()) {
                return std::nullopt;
            }

            const nlohmann::json& choiceMessage = respJson["choices"][0]["message"];
            if (auto text = extractText(choiceMessage)) {
                lastText = text;
            }

            const bool hasToolCalls = choiceMessage.contains("tool_calls") &&
                                      choiceMessage["tool_calls"].is_array() &&
                                      !choiceMessage["tool_calls"].empty();
            if (!hasToolCalls) {
                return lastText;
            }

            messages.push_back(choiceMessage);
            for (const auto& call : choiceMessage["tool_calls"]) {
                const auto toolCallId = call.at("id").get<std::string>();
                const auto toolName =
                    call.at("function").at("name").get<std::string>();

                nlohmann::json toolInput;
                try {
                    toolInput = nlohmann::json::parse(
                        call.at("function").at("arguments").get<std::string>());
                } catch (const std::exception&) {
                    toolInput = nlohmann::json::object();
                }

                bool isError = false;
                std::string result;
                try {
                    result = exec(toolName, toolInput, isError);
                } catch (const std::exception& ex) {
                    result = std::string("Tool execution failed: ") + ex.what();
                }

                messages.push_back({{"role", "tool"},
                                    {"tool_call_id", toolCallId},
                                    {"content", result}});
            }
        }

        return lastText ? lastText
                        : std::optional<std::string>(
                              "Tool call limit reached before completing the "
                              "request.");
    }

   private:
    std::string url_;
    std::string authkey_;
};

}  // namespace llm::openai
