#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using json = nlohmann::json;

namespace openai {

constexpr const char* kOpenAI_API_ChatCompletions_Endpoint =
    "/chat/completions";
constexpr const char* kOpenAI_API_Models_Endpoint = "/models";

// --- 1. Basic Structures ---

struct FunctionCall {
    std::string name;
    std::string arguments;  // usually a JSON string
};

struct ToolCall {
    std::string id;
    std::string type;  // usually "function"
    FunctionCall function;
};

struct Message {
    std::string role;  // "system", "user", "assistant"
    std::string content;
    // Optional: Handling tool calls if the model sends them
    std::vector<ToolCall> tool_calls;

    // Helper constructor for simple text messages
    static Message User(std::string text) {
        return {.role = "user", .content = std::move(text)};
    }
    static Message System(std::string text) {
        return {.role = "system", .content = std::move(text)};
    }
};

struct Usage {
    int prompt_tokens = 0;
    int completion_tokens = 0;
    int total_tokens = 0;
};

struct Model {
    std::string id;
    std::string object;
    std::string owned_by;
};

// --- 2. Request & Response Objects ---

struct ChatRequest {
    std::string model;
    std::vector<Message> messages;
    double temperature = 0.7;
    bool stream = false;
};

struct Choice {
    int index;
    Message message;
    std::string finish_reason;
};

struct ChatResponse {
    std::string id;
    std::string object;
    long long created;
    std::string model;
    std::vector<Choice> choices;
    Usage usage;
};

struct ModelResponse {
    std::vector<Model> data;
};

// --- 3. The "Magic" Macros ---
// These automatically create to_json() and from_json() functions!

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(FunctionCall, name, arguments)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ToolCall, id, type, function)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Model, id, object, owned_by)

// Note: We need a custom handler for Message because 'tool_calls' is
// optional/complex but for simplicity, we can use the macro if we ensure
// default values.
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Message, role, content, tool_calls)

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Usage, prompt_tokens, completion_tokens,
                                   total_tokens)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChatRequest, model, messages, temperature,
                                   stream)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Choice, index, message, finish_reason)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChatResponse, id, object, created, model,
                                   choices, usage)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ModelResponse, data)

}  // namespace openai