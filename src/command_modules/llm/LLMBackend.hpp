#pragma once

#include <cctype>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

// Common interface shared by the three LLM HTTP backends (OpenAI-compatible,
// native LM Studio, Anthropic). Each backend is a thin client over CurlUtils.
namespace llm {

// A model offered by the backend. `id` is what we send back in requests;
// `display` is a human-friendly label (may equal `id`).
struct LLMModel {
    std::string id;
    std::string display;
};

enum class LLMApiType { OpenAI, LMStudio, Anthropic };

// Case-insensitive parse of the configured `ApiType` string.
inline std::optional<LLMApiType> parseApiType(std::string_view value) {
    const auto ieq = [value](std::string_view other) {
        if (other.size() != value.size()) {
            return false;
        }
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (std::tolower(static_cast<unsigned char>(value[i])) !=
                std::tolower(static_cast<unsigned char>(other[i]))) {
                return false;
            }
        }
        return true;
    };
    if (ieq("openai")) {
        return LLMApiType::OpenAI;
    }
    if (ieq("lmstudio")) {
        return LLMApiType::LMStudio;
    }
    if (ieq("anthropic")) {
        return LLMApiType::Anthropic;
    }
    return std::nullopt;
}

struct LLMBackend {
    virtual ~LLMBackend() = default;

    // Returns the models the server offers. Empty vector on failure.
    virtual std::vector<LLMModel> listModels() = 0;

    // Runs a single-turn chat (system prompt + one user message) against the
    // given model. Returns the answer text, or std::nullopt on failure.
    // `chatId` lets stateful backends (LM Studio) thread conversation context.
    virtual std::optional<std::string> chat(const std::string& model,
                                            const std::string& systemPrompt,
                                            const std::string& userInput,
                                            std::int64_t chatId) = 0;
};

// Defined in ask.cpp (the only translation unit that pulls in every backend).
std::unique_ptr<LLMBackend> makeBackend(LLMApiType type, std::string url,
                                        std::string authkey);

}  // namespace llm
