#pragma once

#include <algorithm>
#include <cstdint>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "CurlUtils.hpp"
#include "LLMBackend.hpp"
#include "LMStudioApi.hpp"
#include "OpenAIApi.hpp"

// Native LM Studio REST backend (`/api/v1/models` + `/api/v1/chat`). Uses LM
// Studio's "responses" style with per-chat `response_id` continuity.
namespace llm {

class LMStudioBackend : public LLMBackend {
   public:
    LMStudioBackend(std::string url, std::string authkey,
                    CurlUtils::CancelChecker cancelled = {})
        : url_(std::move(url)),
          authkey_(std::move(authkey)),
          cancelled_(std::move(cancelled)) {}

    std::vector<LLMModel> listModels() override {
        auto raw =
            CurlUtils::download_memory(url_ + LMStudioApi::kModelsEndpoint,
                                       cancelled_, std::string_view{authkey_});
        if (!raw) {
            return {};
        }
        std::vector<LLMModel> out;
        try {
            auto resp =
                nlohmann::json::parse(*raw).get<LMStudioApi::ModelResponse>();
            for (auto& m : resp.models) {
                if (m.type != LMStudioApi::LLMType::llm) {
                    continue;  // skip embedding models
                }
                out.push_back(
                    {m.key, m.display_name.empty() ? m.key : m.display_name});
            }
        } catch (const std::exception&) {
            return {};
        }
        return out;
    }

    std::optional<std::string> chat(const std::string& model,
                                    const std::string& systemPrompt,
                                    const std::string& userInput,
                                    std::int64_t chatId) override {
        LMStudioApi::ChatRequest req;
        req.model = model;
        req.input = userInput;
        req.system_prompt = systemPrompt;
        return runChat(std::move(req), chatId);
    }

    std::optional<std::string> classify(const std::string& model,
                                        const std::string& systemPrompt,
                                        const std::string& userInput) override {
        return runChat(
            LMStudioApi::makeClassifierRequest(model, systemPrompt, userInput),
            std::nullopt);
    }

    // LM Studio's native /api/v1/chat takes no client-supplied tool schemas -
    // its `integrations` field only selects MCP servers already installed in
    // the app. Inheriting the base class default here would silently discard
    // every tool the router selected, so route tool-calling turns through the
    // OpenAI-compatible endpoint LM Studio serves from the same base URL
    // (/v1/chat/completions), which does accept them.
    //
    // Trade-off: that endpoint is stateless, so a tool-calling turn neither
    // reads nor advances this chat's response_id. The tool exchange stays out
    // of LM Studio's stored conversation and the next plain turn resumes from
    // the response id before it - the same history-free behaviour the OpenAI
    // and Anthropic backends already have on every turn.
    std::optional<std::string> chat(const std::string& model,
                                    const std::string& systemPrompt,
                                    const std::string& userInput,
                                    std::int64_t chatId,
                                    const std::vector<Tool>& tools,
                                    ToolExecutor exec) override {
        if (tools.empty()) {
            return chat(model, systemPrompt, userInput, chatId);
        }
        openai::OpenAIBackend compat(url_, authkey_, cancelled_);
        return compat.chat(model, systemPrompt, userInput, chatId, tools,
                           std::move(exec));
    }

   private:
    std::optional<std::string> runChat(LMStudioApi::ChatRequest req,
                                       std::optional<std::int64_t> chatId) {
        std::unique_lock<std::mutex> turnLock;
        if (chatId) {
            turnLock = std::unique_lock(*conversationMutex(*chatId));
            if (auto previous = previousResponseId(*chatId, req.model)) {
                req.previous_response_id = *previous;
            }
        }
        const std::string model = req.model;
        nlohmann::json payload = req;

        auto raw = CurlUtils::send_json_get_reply(
            url_ + LMStudioApi::kChatEndpoint, payload.dump(),
            std::string_view{authkey_}, cancelled_);
        if (!raw) {
            return std::nullopt;
        }
        try {
            auto resp =
                nlohmann::json::parse(*raw).get<LMStudioApi::ChatResponse>();
            if (chatId) {
                rememberResponseId(*chatId, model, resp.response_id);
            }
            auto it = std::ranges::find_if(
                resp.output, [](const LMStudioApi::ChatResponse::Output& o) {
                    return o.type == "message";
                });
            if (it == resp.output.end()) {
                return std::nullopt;
            }
            return it->content;
        } catch (const std::exception&) {
            return std::nullopt;
        }
    }

    // Per-chat conversation continuity, shared across backend instances (a new
    // backend is built per command invocation).
    struct ConversationState {
        std::string model;
        std::string responseId;
        std::shared_ptr<std::mutex> turnMutex = std::make_shared<std::mutex>();
    };

    static std::shared_ptr<std::mutex> conversationMutex(std::int64_t chatId) {
        auto [mtx, map] = store();
        const std::lock_guard lock(mtx);
        return map[chatId].turnMutex;
    }
    static std::optional<std::string> previousResponseId(
        std::int64_t chatId, std::string_view model) {
        auto [mtx, map] = store();
        const std::lock_guard lock(mtx);
        if (auto it = map.find(chatId); it != map.end() &&
                                        it->second.model == model &&
                                        !it->second.responseId.empty()) {
            return it->second.responseId;
        }
        return std::nullopt;
    }
    static void rememberResponseId(std::int64_t chatId, std::string_view model,
                                   const std::optional<std::string>& id) {
        auto [mtx, map] = store();
        const std::lock_guard lock(mtx);
        auto& state = map[chatId];
        if (id) {
            state.model = model;
            state.responseId = *id;
        } else {
            state.model.clear();
            state.responseId.clear();
        }
    }
    static std::pair<std::mutex&,
                     std::unordered_map<std::int64_t, ConversationState>&>
    store() {
        static std::mutex mtx;
        static std::unordered_map<std::int64_t, ConversationState> map;
        return {mtx, map};
    }

    std::string url_;
    std::string authkey_;
    CurlUtils::CancelChecker cancelled_;
};

}  // namespace llm
