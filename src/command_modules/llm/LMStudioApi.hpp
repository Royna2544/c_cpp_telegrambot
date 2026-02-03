#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace nlohmann {
// 1. Support for std::optional (Maps std::nullopt to JSON null)
template <typename T>
struct adl_serializer<std::optional<T>> {
    static void to_json(json& j, const std::optional<T>& opt) {
        if (opt) {
            j = *opt;
        } else {
            j = nullptr;
        }
    }
    static void from_json(const json& j, std::optional<T>& opt) {
        if (j.is_null()) {
            opt = std::nullopt;
        } else {
            opt = j.get<T>();
        }
    }
};

// 2. Support for std::monostate (Maps to JSON null)
template <>
struct adl_serializer<std::monostate> {
    static void to_json(json& j, const std::monostate&) { j = nullptr; }
    static void from_json(const json& j, std::monostate&) {
        if (!j.is_null())
            throw json::type_error::create(302, "type must be null", &j);
    }
};

// 3. Support for std::variant (Tries types in order)
template <typename... Ts>
struct adl_serializer<std::variant<Ts...>> {
    static void to_json(json& j, const std::variant<Ts...>& data) {
        std::visit([&j](const auto& v) { j = v; }, data);
    }

    static void from_json(const json& j, std::variant<Ts...>& data) {
        // Helper to try parsing each type in the variant
        if (!try_parse<Ts...>(j, data)) {
            throw json::type_error::create(302, "Could not parse variant", &j);
        }
    }

   private:
    template <typename T, typename... Args>
    static bool try_parse(const json& j, std::variant<Ts...>& data) {
        try {
            // If T is monostate, only accept null
            if constexpr (std::is_same_v<T, std::monostate>) {
                if (j.is_null()) {
                    data = std::monostate{};
                    return true;
                }
                return try_parse<Args...>(j, data);
            }

            // Otherwise try to convert
            data = j.get<T>();
            return true;
        } catch (...) {
            if constexpr (sizeof...(Args) > 0) {
                return try_parse<Args...>(j, data);
            }
            return false;
        }
    }
};
}  // namespace nlohmann

namespace LMStudioApi {

// --- Enums ---
enum class LLMType : std::uint8_t {
    llm,
    embedding,
};
NLOHMANN_JSON_SERIALIZE_ENUM(LLMType, {
                                          {LLMType::llm, "llm"},
                                          {LLMType::embedding, "embedding"},
                                      })

enum class LLMFormat : std::uint8_t { gguf, mlx };
NLOHMANN_JSON_SERIALIZE_ENUM(LLMFormat, {
                                            {LLMFormat::gguf, "gguf"},
                                            {LLMFormat::mlx, "mlx"},
                                        })

// --- Model Structure ---
struct Model {
    // Type of model
    LLMType type;

    // Model publisher name.
    std::string publisher;

    // Unique identifier for the model.
    std::string key;

    // Human-readable name of the model.
    std::string display_name;

    // Model architecture (e.g. "llama", "mistral", etc.). Absent for embedding
    // models.
    std::optional<std::string> architecture;

    // Quantization informations for this model.
    struct Quantization {
        // Quatization method name
        std::string name;
        // Bits per weight for the quantization
        int bits_per_weight;
    } quantization;

    // Size of the model file in bytes.
    size_t size_bytes;

    // Human-readable parameter count (e.g. "7B", "13B", etc.).
    std::string params_string;

    struct LoadedInstance {
        // Unique identifier for the loaded model instance.
        std::string id;

        // Configurations for the loaded instance.
        struct Config {
            // The maximum context length supported by the model in number of
            // tokens.
            int64_t context_length;

            // Number of input tokens to process together in a single batch
            // during evaluation. May be absent if the model does not support
            // batching.
            std::optional<int> eval_batch_size;

            // Whether Flash Attention is enabled for this model instance.
            std::optional<bool> flash_attention;

            // Number of experts in the model (for mixture of experts models).
            std::optional<int> num_experts;

            // Whether KV cache is offloaded to GPU memory. Absent for embedding
            // models.
            std::optional<bool> offload_kv_cache_to_gpu;
        } config;
    };

    std::vector<LoadedInstance> loaded_instances;

    // Maximum context length supported by this model instance in number of
    // tokens.
    int max_context_length;

    // Format of the model file.
    LLMFormat format;

    struct Capabilities {
        bool vision;
        bool trained_for_tool_use;
    };

    // Model capabilities. Absent for embedding models.
    std::optional<Capabilities> capabilities;

    // Model description. Absent for embedding models.
    std::optional<std::string> description;
};

struct ModelResponse {
    // Array of available models on the server.
    std::vector<Model> models;
};

enum class Reasoning : std::uint8_t { off, low, medium, high, on };

NLOHMANN_JSON_SERIALIZE_ENUM(Reasoning, {
                                            {Reasoning::off, "off"},
                                            {Reasoning::low, "low"},
                                            {Reasoning::medium, "medium"},
                                            {Reasoning::high, "high"},
                                            {Reasoning::on, "on"},
                                        })

struct ChatRequest {
    // Unique identifier of the model to use for generating the response.
    std::string model;

    // Object representing a message with additional metadata.
    struct InputObject {
        // "message", "image", Type of input item.
        std::string type;
        // Content of the input item. For "message" type, this is the text
        // For "image" type, this could be a URL or base64-encoded data.
        std::string content;
    };

    // Input can be either a simple string (text message) or a list of input
    std::variant<std::string, std::vector<InputObject>> input;

    // System message that sets the behavior of the assistant.
    std::optional<std::string> system_prompt;

    // Specification of a plugin to use. Plugins contain ``mcp.json`` installed
    // MCP servers (id mcp/<server_label>)
    struct Plugin {
        // Type of integration
        std::string type = "plugin";
        // Unique identifier of the plugin
        std::string id;
        // List of tool names the model can call from this plugin. If not
        // provided, all tools from the plugin are allowed.
        std::optional<std::vector<std::string>> allowed_tools;
    };

    // OMIT Ephemeral plugins are not supported yet.
    std::variant<std::monostate, std::vector<std::string>, std::vector<Plugin>>
        integrations = std::monostate{};

    // Whether to stream partial outputs via SSE. Default false. See  for more
    // information.
    std::optional<bool> stream;

    // Randomness in token selection. 0 is deterministic, higher values increase
    // creativity [0,1].
    std::optional<float> temperature;

    // Minimum cumulative probability for the possible next tokens [0,1].
    std::optional<float> top_p;

    // Limits next token selection to top-k most probable tokens.
    std::optional<int> top_k;

    // Minimum base probability for a token to be selected for output [0,1].
    std::optional<float> min_p;

    // Penalty for repeating token sequences. 1 is no penalty, higher values
    // discourage repetition.
    std::optional<float> repeat_penalty;

    // Maximum number of tokens to generate.
    std::optional<int> max_output_tokens;

    // Reasoning setting. Will error if the model being used does not support
    // the reasoning setting using. Defaults to the automatically chosen setting
    // for the model.
    std::optional<Reasoning> reasoning;

    // Number of tokens to consider as context. Higher values recommended for
    // MCP usage.
    std::optional<int> context_length;

    // Whether to store the chat. If set, response will return a "response_id"
    // field. Default true.
    std::optional<bool> store;

    // Identifier of existing response to append to. Must start with "resp_".
    std::optional<std::string> previous_response_id;
};

struct ChatResponse {
    // Unique identifier for the loaded model instance that generated the
    // response.
    std::string model_instance_id;

    struct Output {
        // "message", "tool_call", "reasoning", "invalid_tool_call"
        std::string type;
        // Content of the output item.
        std::string content;
    };

    // Array of output items generated. Each item can be one of three types.
    std::vector<Output> output;

    struct Stats {
        // Number of input tokens. Includes formatting, tool definitions, and
        // prior messages in the chat.
        int input_tokens;

        // Total number of output tokens generated.
        int total_output_tokens;

        // Number of tokens used for reasoning.
        int reasoning_output_tokens;

        // Generation speed in tokens per second.
        float tokens_per_second;

        // Time in seconds to generate the first token.
        float time_to_first_token_seconds;

        // Time taken to load the model for this request in seconds. Present
        // only if the model was not already loaded.
        std::optional<float> model_load_time_seconds;
    } stats;

    // Identifier of the response for subsequent requests. Starts with
    // "resp_". Present when store is true.
    std::optional<std::string> response_id;
};

// API endpoints
constexpr const char* kModelsEndpoint = "/api/v1/models";
constexpr const char* kChatEndpoint = "/api/v1/chat";

// --- Macro Definitions (Must be outside the struct) ---
// 1. Model::Quantization
inline void to_json(nlohmann::json& j, const Model::Quantization& p) {
    j = nlohmann::json{{"name", p.name},
                       {"bits_per_weight", p.bits_per_weight}};
}
inline void from_json(const nlohmann::json& j, Model::Quantization& p) {
    if (j.contains("name") && !j["name"].is_null()) j.at("name").get_to(p.name);
    if (j.contains("bits_per_weight") && !j["bits_per_weight"].is_null())
        j.at("bits_per_weight").get_to(p.bits_per_weight);
}

// 2. Model::Capabilities
inline void to_json(nlohmann::json& j, const Model::Capabilities& p) {
    j = nlohmann::json{{"vision", p.vision},
                       {"trained_for_tool_use", p.trained_for_tool_use}};
}
inline void from_json(const nlohmann::json& j, Model::Capabilities& p) {
    // We use .value() to provide safe defaults if missing
    p.vision = j.value("vision", false);
    p.trained_for_tool_use = j.value("trained_for_tool_use", false);
}

// 3. Model::LoadedInstance::Config
inline void to_json(nlohmann::json& j, const Model::LoadedInstance::Config& p) {
    j = nlohmann::json{{"context_length", p.context_length}};

    // Only write optional fields if they have values
    if (p.eval_batch_size) j["eval_batch_size"] = p.eval_batch_size;
    if (p.flash_attention) j["flash_attention"] = p.flash_attention;
    if (p.num_experts) j["num_experts"] = p.num_experts;
    if (p.offload_kv_cache_to_gpu)
        j["offload_kv_cache_to_gpu"] = p.offload_kv_cache_to_gpu;
}
inline void from_json(const nlohmann::json& j,
                      Model::LoadedInstance::Config& p) {
    if (j.contains("context_length"))
        j.at("context_length").get_to(p.context_length);
    if (j.contains("eval_batch_size"))
        j.at("eval_batch_size").get_to(p.eval_batch_size);
    if (j.contains("flash_attention"))
        j.at("flash_attention").get_to(p.flash_attention);
    if (j.contains("num_experts")) j.at("num_experts").get_to(p.num_experts);
    if (j.contains("offload_kv_cache_to_gpu"))
        j.at("offload_kv_cache_to_gpu").get_to(p.offload_kv_cache_to_gpu);
}

// 4. Model::LoadedInstance
inline void to_json(nlohmann::json& j, const Model::LoadedInstance& p) {
    j = nlohmann::json{{"id", p.id}, {"config", p.config}};
}
inline void from_json(const nlohmann::json& j, Model::LoadedInstance& p) {
    if (j.contains("id")) j.at("id").get_to(p.id);
    if (j.contains("config")) j.at("config").get_to(p.config);
}

// 5. Model (Top Level)
inline void to_json(nlohmann::json& j, const Model& p) {
    j = nlohmann::json{
        {"type", p.type},
        {"publisher", p.publisher},
        {"key", p.key},
        {"display_name", p.display_name},
        {"quantization", p.quantization},  // Recursive call
        {"size_bytes", p.size_bytes},
        {"params_string", p.params_string},
        {"loaded_instances", p.loaded_instances},  // Recursive vector call
        {"max_context_length", p.max_context_length},
        {"format", p.format}};

    // Optionals: Only add to JSON if they exist
    if (p.architecture) j["architecture"] = p.architecture;
    if (p.capabilities) j["capabilities"] = p.capabilities;
    if (p.description) j["description"] = p.description;
}

inline void from_json(const nlohmann::json& j, Model& p) {
    // Required fields (or safe to read with defaults)
    if (j.contains("type")) j.at("type").get_to(p.type);
    if (j.contains("publisher")) j.at("publisher").get_to(p.publisher);
    if (j.contains("key")) j.at("key").get_to(p.key);
    if (j.contains("display_name")) j.at("display_name").get_to(p.display_name);

    // Structs (Recursive)
    if (j.contains("quantization") && !j["quantization"].is_null())
        j.at("quantization").get_to(p.quantization);
    if (j.contains("loaded_instances"))
        j.at("loaded_instances").get_to(p.loaded_instances);

    // Primitives
    p.size_bytes = j.value("size_bytes", 0);
    if (!j["params_string"].is_null())
        p.params_string = j.value("params_string", "");
    p.max_context_length = j.value("max_context_length", 0);

    if (j.contains("format") && !j["format"].is_null())
        j.at("format").get_to(p.format);

    // Optionals
    if (j.contains("architecture") && !j["architecture"].is_null())
        j.at("architecture").get_to(p.architecture);
    if (j.contains("capabilities")) j.at("capabilities").get_to(p.capabilities);
    if (j.contains("description")) j.at("description").get_to(p.description);
}  // --- ChatRequest Internal Structs ---

inline void to_json(nlohmann::json& j, const ChatRequest::InputObject& p) {
    j = nlohmann::json{{"type", p.type}, {"content", p.content}};
}
inline void from_json(const nlohmann::json& j, ChatRequest::InputObject& p) {
    if (j.contains("type")) j.at("type").get_to(p.type);
    if (j.contains("content")) j.at("content").get_to(p.content);
}

inline void to_json(nlohmann::json& j, const ChatRequest::Plugin& p) {
    j = nlohmann::json{{"type", p.type}, {"id", p.id}};
    if (p.allowed_tools) j["allowed_tools"] = p.allowed_tools;
}
inline void from_json(const nlohmann::json& j, ChatRequest::Plugin& p) {
    p.type = j.value("type", "plugin");
    if (j.contains("id")) j.at("id").get_to(p.id);
    if (j.contains("allowed_tools"))
        j.at("allowed_tools").get_to(p.allowed_tools);
}

// --- ChatRequest Top Level ---

inline void to_json(nlohmann::json& j, const ChatRequest& p) {
    j = nlohmann::json{{"model", p.model}};

    // Handle 'input' variant
    if (std::holds_alternative<std::string>(p.input)) {
        j["input"] = std::get<std::string>(p.input);
    } else {
        j["input"] = std::get<std::vector<ChatRequest::InputObject>>(p.input);
    }

    if (p.system_prompt) j["system_prompt"] = p.system_prompt;

    // Handle 'integrations' variant
    if (std::holds_alternative<std::vector<std::string>>(p.integrations)) {
        j["integrations"] = std::get<std::vector<std::string>>(p.integrations);
    } else if (std::holds_alternative<std::vector<ChatRequest::Plugin>>(
                   p.integrations)) {
        j["integrations"] =
            std::get<std::vector<ChatRequest::Plugin>>(p.integrations);
    } else {
        j["integrations"] = nullptr;  // monostate
    }

    // Optionals
    if (p.stream) j["stream"] = p.stream;
    if (p.temperature) j["temperature"] = p.temperature;
    if (p.top_p) j["top_p"] = p.top_p;
    if (p.top_k) j["top_k"] = p.top_k;
    if (p.min_p) j["min_p"] = p.min_p;
    if (p.repeat_penalty) j["repeat_penalty"] = p.repeat_penalty;
    if (p.max_output_tokens) j["max_output_tokens"] = p.max_output_tokens;
    if (p.reasoning) j["reasoning"] = p.reasoning;
    if (p.context_length) j["context_length"] = p.context_length;
    if (p.store) j["store"] = p.store;
    if (p.previous_response_id)
        j["previous_response_id"] = p.previous_response_id;
}

inline void from_json(const nlohmann::json& j, ChatRequest& p) {
    if (j.contains("model")) j.at("model").get_to(p.model);

    // Manual Logic for 'input' Variant
    if (j.contains("input")) {
        const auto& input_json = j["input"];
        if (input_json.is_string()) {
            p.input = input_json.get<std::string>();
        } else if (input_json.is_array()) {
            p.input = input_json.get<std::vector<ChatRequest::InputObject>>();
        }
    }

    if (j.contains("system_prompt"))
        j.at("system_prompt").get_to(p.system_prompt);

    // Manual Logic for 'integrations' Variant
    if (j.contains("integrations")) {
        const auto& int_json = j["integrations"];
        if (int_json.is_array() && !int_json.empty()) {
            // Peek at the first element to decide type
            if (int_json[0].is_string()) {
                p.integrations = int_json.get<std::vector<std::string>>();
            } else if (int_json[0].is_object()) {
                p.integrations =
                    int_json.get<std::vector<ChatRequest::Plugin>>();
            }
        } else {
            // Null or empty array -> monostate
            p.integrations = std::monostate{};
        }
    }

    // Optionals
    if (j.contains("stream")) j.at("stream").get_to(p.stream);
    if (j.contains("temperature")) j.at("temperature").get_to(p.temperature);
    if (j.contains("top_p")) j.at("top_p").get_to(p.top_p);
    if (j.contains("top_k")) j.at("top_k").get_to(p.top_k);
    if (j.contains("min_p")) j.at("min_p").get_to(p.min_p);
    if (j.contains("repeat_penalty"))
        j.at("repeat_penalty").get_to(p.repeat_penalty);
    if (j.contains("max_output_tokens"))
        j.at("max_output_tokens").get_to(p.max_output_tokens);
    if (j.contains("reasoning")) j.at("reasoning").get_to(p.reasoning);
    if (j.contains("context_length"))
        j.at("context_length").get_to(p.context_length);
    if (j.contains("store")) j.at("store").get_to(p.store);
    if (j.contains("previous_response_id"))
        j.at("previous_response_id").get_to(p.previous_response_id);
}

// --- ChatResponse Internal Structs ---

inline void to_json(nlohmann::json& j, const ChatResponse::Output& p) {
    j = nlohmann::json{{"type", p.type}, {"content", p.content}};
}
inline void from_json(const nlohmann::json& j, ChatResponse::Output& p) {
    if (j.contains("type")) j.at("type").get_to(p.type);
    if (j.contains("content")) j.at("content").get_to(p.content);
}

inline void to_json(nlohmann::json& j, const ChatResponse::Stats& p) {
    j = nlohmann::json{
        {"input_tokens", p.input_tokens},
        {"total_output_tokens", p.total_output_tokens},
        {"reasoning_output_tokens", p.reasoning_output_tokens},
        {"tokens_per_second", p.tokens_per_second},
        {"time_to_first_token_seconds", p.time_to_first_token_seconds}};
    if (p.model_load_time_seconds)
        j["model_load_time_seconds"] = p.model_load_time_seconds;
}
inline void from_json(const nlohmann::json& j, ChatResponse::Stats& p) {
    p.input_tokens = j.value("input_tokens", 0);
    p.total_output_tokens = j.value("total_output_tokens", 0);
    p.reasoning_output_tokens = j.value("reasoning_output_tokens", 0);
    p.tokens_per_second = j.value("tokens_per_second", 0.0f);
    p.time_to_first_token_seconds =
        j.value("time_to_first_token_seconds", 0.0f);
    if (j.contains("model_load_time_seconds"))
        j.at("model_load_time_seconds").get_to(p.model_load_time_seconds);
}

// --- ChatResponse Top Level ---

inline void to_json(nlohmann::json& j, const ChatResponse& p) {
    j = nlohmann::json{{"model_instance_id", p.model_instance_id},
                       {"output", p.output},
                       {"stats", p.stats}};
    if (p.response_id) j["response_id"] = p.response_id;
}
inline void from_json(const nlohmann::json& j, ChatResponse& p) {
    if (j.contains("model_instance_id"))
        j.at("model_instance_id").get_to(p.model_instance_id);
    if (j.contains("output")) j.at("output").get_to(p.output);
    if (j.contains("stats")) j.at("stats").get_to(p.stats);
    if (j.contains("response_id")) j.at("response_id").get_to(p.response_id);
}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ModelResponse, models)

}  // namespace LMStudioApi
