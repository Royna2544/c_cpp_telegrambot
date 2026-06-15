#pragma once

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "CurlUtils.hpp"
#include "LLMBackend.hpp"
#include "LMStudioApi.hpp"

// Native LM Studio REST backend (`/api/v1/models` + `/api/v1/chat`). Uses LM
// Studio's "responses" style with per-chat `response_id` continuity.
namespace llm {

class LMStudioBackend : public LLMBackend {
   public:
    LMStudioBackend(std::string url, std::string authkey)
        : url_(std::move(url)), authkey_(std::move(authkey)) {}

    std::vector<LLMModel> listModels() override {
        auto raw = CurlUtils::download_memory(
            url_ + LMStudioApi::kModelsEndpoint, nullptr,
            std::string_view{authkey_});
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
        if (auto prev = previousResponseId(chatId)) {
            req.previous_response_id = *prev;
        }
        nlohmann::json payload = req;

        auto raw = CurlUtils::send_json_get_reply(
            url_ + LMStudioApi::kChatEndpoint, payload.dump(),
            std::string_view{authkey_});
        if (!raw) {
            return std::nullopt;
        }
        try {
            auto resp =
                nlohmann::json::parse(*raw).get<LMStudioApi::ChatResponse>();
            rememberResponseId(chatId, resp.response_id);
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

   private:
    // Per-chat conversation continuity, shared across backend instances (a new
    // backend is built per command invocation).
    static std::optional<std::string> previousResponseId(std::int64_t chatId) {
        auto [mtx, map] = store();
        const std::lock_guard lock(mtx);
        if (auto it = map.find(chatId); it != map.end() && !it->second.empty()) {
            return it->second;
        }
        return std::nullopt;
    }
    static void rememberResponseId(std::int64_t chatId,
                                   const std::optional<std::string>& id) {
        auto [mtx, map] = store();
        const std::lock_guard lock(mtx);
        if (id) {
            map[chatId] = *id;
        } else {
            map.erase(chatId);
        }
    }
    static std::pair<std::mutex&, std::unordered_map<std::int64_t, std::string>&>
    store() {
        static std::mutex mtx;
        static std::unordered_map<std::int64_t, std::string> map;
        return {mtx, map};
    }

    std::string url_;
    std::string authkey_;
};

}  // namespace llm
