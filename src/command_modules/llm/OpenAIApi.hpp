#pragma once

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
            auto list = nlohmann::json::parse(*raw).get<ModelList>();
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
            auto resp = nlohmann::json::parse(*raw).get<ChatResponse>();
            if (resp.choices.empty()) {
                return std::nullopt;
            }
            return resp.choices.front().message.content;
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

   private:
    std::string url_;
    std::string authkey_;
};

}  // namespace llm::openai
