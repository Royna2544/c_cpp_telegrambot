#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

#include "CurlUtils.hpp"
#include "LLMBackend.hpp"

// Anthropic Messages API backend. Unlike the OpenAI/LM Studio backends this
// authenticates with `x-api-key` + `anthropic-version` headers (not a bearer
// token), hence the header-list CurlUtils overloads.
namespace llm::anthropic {

constexpr const char* kModelsEndpoint = "/v1/models";
constexpr const char* kMessagesEndpoint = "/v1/messages";
constexpr const char* kVersionHeader = "anthropic-version: 2023-06-01";
constexpr int kMaxTokens = 4096;
constexpr int kMaxToolIterations = 6;

// --- Request ---
struct Message {
    std::string role;
    std::string content;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Message, role, content)

struct MessagesRequest {
    std::string model;
    int max_tokens{kMaxTokens};  // required by the API
    std::string system;
    std::vector<Message> messages;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MessagesRequest, model, max_tokens, system,
                                   messages)

// --- Response ---
struct ContentBlock {
    std::string type;
    std::string text;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ContentBlock, type, text)

struct MessagesResponse {
    std::vector<ContentBlock> content;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(MessagesResponse, content)

struct ModelEntry {
    std::string id;
    std::string display_name;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ModelEntry, id, display_name)

struct ModelList {
    std::vector<ModelEntry> data;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ModelList, data)

class AnthropicBackend : public LLMBackend {
   public:
    AnthropicBackend(std::string url, std::string authkey)
        : url_(std::move(url)), authkey_(std::move(authkey)) {}

    std::vector<LLMModel> listModels() override {
        auto raw = CurlUtils::download_memory(url_ + kModelsEndpoint, nullptr,
                                              headers());
        if (!raw) {
            return {};
        }
        std::vector<LLMModel> out;
        try {
            auto list = nlohmann::json::parse(*raw).get<ModelList>();
            for (auto& m : list.data) {
                out.push_back(
                    {m.id, m.display_name.empty() ? m.id : m.display_name});
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
        MessagesRequest req;
        req.model = model;
        req.system = systemPrompt;
        req.messages = {{"user", userInput}};
        nlohmann::json payload = req;

        auto raw = CurlUtils::send_json_get_reply(url_ + kMessagesEndpoint,
                                                  payload.dump(), headers());
        if (!raw) {
            return std::nullopt;
        }
        try {
            auto resp = nlohmann::json::parse(*raw).get<MessagesResponse>();
            for (auto& block : resp.content) {
                if (block.type == "text") {
                    return block.text;
                }
            }
            return std::nullopt;
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    // Tool-calling variant: offers `tools` to the model and dispatches
    // `tool_use` blocks through `exec`, looping (raw JSON, since tool_use /
    // tool_result content blocks don't fit the plain-string Message shape
    // above) until the model returns a final answer or kMaxToolIterations is
    // exhausted.
    std::optional<std::string> chat(const std::string& model,
                                    const std::string& systemPrompt,
                                    const std::string& userInput,
                                    std::int64_t /*chatId*/,
                                    const std::vector<llm::Tool>& tools,
                                    llm::ToolExecutor exec) override {
        nlohmann::json messages = nlohmann::json::array(
            {{{"role", "user"}, {"content", userInput}}});

        nlohmann::json toolsJson = nlohmann::json::array();
        for (const auto& tool : tools) {
            toolsJson.push_back({{"name", tool.name},
                                 {"description", tool.description},
                                 {"input_schema", tool.inputSchema}});
        }

        std::optional<std::string> lastText;
        for (int iteration = 0; iteration < kMaxToolIterations; ++iteration) {
            nlohmann::json payload{{"model", model},
                                   {"max_tokens", kMaxTokens},
                                   {"system", systemPrompt},
                                   {"messages", messages}};
            if (!toolsJson.empty()) {
                payload["tools"] = toolsJson;
            }

            auto raw = CurlUtils::send_json_get_reply(
                url_ + kMessagesEndpoint, payload.dump(), headers());
            if (!raw) {
                return std::nullopt;
            }

            nlohmann::json respJson;
            try {
                respJson = nlohmann::json::parse(*raw);
            } catch (const std::exception&) {
                return std::nullopt;
            }
            if (!respJson.contains("content") ||
                !respJson["content"].is_array()) {
                return std::nullopt;
            }
            const nlohmann::json& content = respJson["content"];

            nlohmann::json toolUseBlocks = nlohmann::json::array();
            for (const auto& block : content) {
                const auto type = block.value("type", std::string{});
                if (type == "text") {
                    lastText = block.value("text", std::string{});
                } else if (type == "tool_use") {
                    toolUseBlocks.push_back(block);
                }
            }

            if (toolUseBlocks.empty()) {
                return lastText;
            }

            messages.push_back({{"role", "assistant"}, {"content", content}});

            nlohmann::json toolResults = nlohmann::json::array();
            for (const auto& block : toolUseBlocks) {
                const auto toolUseId = block.at("id").get<std::string>();
                const auto toolName = block.at("name").get<std::string>();
                const auto toolInput = block.value(
                    "input", nlohmann::json::object());

                bool isError = false;
                std::string result;
                try {
                    result = exec(toolName, toolInput, isError);
                } catch (const std::exception& ex) {
                    isError = true;
                    result = std::string("Tool execution failed: ") + ex.what();
                }

                toolResults.push_back({{"type", "tool_result"},
                                       {"tool_use_id", toolUseId},
                                       {"content", result},
                                       {"is_error", isError}});
            }
            messages.push_back({{"role", "user"}, {"content", toolResults}});
        }

        return lastText ? lastText
                        : std::optional<std::string>(
                              "Tool call limit reached before completing the "
                              "request.");
    }

   private:
    [[nodiscard]] std::vector<std::string> headers() const {
        std::vector<std::string> h{kVersionHeader};
        if (!authkey_.empty()) {
            h.push_back("x-api-key: " + authkey_);
        }
        return h;
    }

    std::string url_;
    std::string authkey_;
};

}  // namespace llm::anthropic
