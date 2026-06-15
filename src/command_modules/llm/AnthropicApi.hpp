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
