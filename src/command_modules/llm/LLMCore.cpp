#include "LLMCore.hpp"

#include <absl/log/log.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "llama.h"

struct LLMCore::Model::Impl {
    llama_model* model = nullptr;
    llama_context* ctx = nullptr;
    llama_sampler* sampler = nullptr;
    llama_batch batch{};
    int n_past = 0;
    int seq_id = 0;
};

LLMCore::Model::Model() { impl_ = std::make_unique<Impl>(); }
LLMCore::Model::~Model() { cleanup(); }

bool LLMCore::Model::load(const std::filesystem::path& modelPath,
                          int n_gpu_layers, int n_ctx, int n_batch) {
    llama_model_params model_params = llama_model_default_params();
    llama_context_params ctx_params = llama_context_default_params();

    DLOG(INFO) << "Model::load called with path: " << modelPath.string()
               << ", n_gpu_layers: " << n_gpu_layers << ", n_ctx: " << n_ctx
               << ", n_batch: " << n_batch;

    model_params.n_gpu_layers = n_gpu_layers;
    ctx_params.n_ctx = n_ctx;
    ctx_params.n_batch = n_batch;

    impl_->model =
        llama_model_load_from_file(modelPath.string().c_str(), model_params);
    if (impl_->model == nullptr) {
        LOG(ERROR) << "Failed to load model from " << modelPath.string();
        return false;
    }
    impl_->ctx = llama_init_from_model(impl_->model, ctx_params);
    if (impl_->ctx == nullptr) {
        LOG(ERROR) << "Failed to create context for model at "
                   << modelPath.string();
        llama_model_free(impl_->model);
        impl_->model = nullptr;
        return false;
    }

    // Initialize Sampler (Greedy)
    auto sparams = llama_sampler_chain_default_params();
    impl_->sampler = llama_sampler_chain_init(sparams);
    llama_sampler_chain_add(impl_->sampler, llama_sampler_init_greedy());

    // Prepare Batch
    impl_->batch = llama_batch_init(n_batch, 0, 1);

    info();

    return true;
}

LLMCore::Model::Info LLMCore::Model::info() const {
    Info info;
    const llama_model* model = impl_->model;
    // Get Parameter Count and File Size
    info.parameterCount = llama_model_n_params(model);
    info.fileSizeBytes = llama_model_size(model);
    // Helper to extract string metadata by Key
    auto get_meta = [&](const char* key) -> std::string {
        // First call with null buffer to get length
        int len = llama_model_meta_val_str(model, key, nullptr, 0);
        if (len < 0) {
            return "N/A";
        }
        std::string buffer(len + 1, '\0');
        llama_model_meta_val_str(model, key, buffer.data(), len + 1);
        return buffer;
    };

    // Fill in metadata
    info.name = get_meta("general.name");
    info.architecture = get_meta("general.architecture");
    info.description = get_meta("general.description");
    return info;
}

void LLMCore::Model::cleanup() {
    if (impl_->ctx != nullptr) {
        llama_batch_free(impl_->batch);
        llama_sampler_free(impl_->sampler);
        llama_free(impl_->ctx);
        impl_->ctx = nullptr;
    }
    if (impl_->model != nullptr) {
        llama_model_free(impl_->model);
        impl_->model = nullptr;
    }
}

namespace {

// 1. Helper to "clear" a batch (reset counters)
void llama_batch_clear(llama_batch& batch) { batch.n_tokens = 0; }

// 2. Helper to add a token to the batch
void llama_batch_add(llama_batch& batch, llama_token id, llama_pos pos,
                     std::vector<llama_seq_id> seq_ids, bool logits) {
    batch.token[batch.n_tokens] = id;
    batch.pos[batch.n_tokens] = pos;
    batch.n_seq_id[batch.n_tokens] = seq_ids.size();
    for (size_t i = 0; i < seq_ids.size(); ++i) {
        batch.seq_id[batch.n_tokens][i] = seq_ids[i];
    }
    batch.logits[batch.n_tokens] = logits;
    batch.n_tokens++;
}
// ----------------------------------------------------

// Helper: Convert string to tokens
std::vector<llama_token> tokenize(const llama_model* model,
                                  const std::string& text) {
    // FIX 1: Get the vocab pointer from the model
    const llama_vocab* vocab = llama_model_get_vocab(model);

    // Calculate required size
    int n_tokens = -1 * llama_tokenize(vocab, text.c_str(), text.length(), NULL,
                                       0, true, false);

    std::vector<llama_token> tokens(n_tokens);
    if (llama_tokenize(vocab, text.c_str(), text.length(), tokens.data(),
                       tokens.size(), true, false) < 0) {
        return {};
    }
    return tokens;
}

}  // namespace

LLMCore::LLMCore() {
    DLOG(INFO) << "LLMCore Constructor called";
    llama_backend_init();
}

LLMCore::~LLMCore() {
    DLOG(INFO) << "LLMCore Destructor called";
    llama_backend_free();
}

constexpr const char* SYSTEM_PROMPT =
    R"(You are Miku, a helpful and friendly AI assistant deployed on Telegram.

### Core Instructions
1. **Mandatory Greeting**: You must start *every* response with the exact phrase: "Hello, I am your Miku!".
2. **Language**: Respond strictly in English.

### Formatting Rules for Telegram
1. **Code Blocks**: Always use triple backticks with a language identifier for code (e.g., ```python). This is essential for Telegram's "click to copy" feature.
2. **Readability**: 
   - Telegram messages are often read on narrow mobile screens. Keep paragraphs short.
   - Use lists (bullet points) instead of complex tables, as tables often break on mobile devices.
   - Use **bold** for emphasis, but avoid excessive formatting.
3. **Length Constraints**: 
   - Telegram has a hard limit of 4096 characters per message. 
   - Be concise. If a detailed answer requires more length, break it down or ask the user if they want the rest in a second message.

### Persona
- Be helpful, polite, and efficient.)";
std::optional<LLMCore::Model::Response> LLMCore::query(
    const Model* model, const std::string_view prompt, int max_tokens) {

    auto start_time = std::chrono::steady_clock::now();
    LOG(INFO) << "LLMCore::query called with prompt: " << prompt;

    // -----------------------
    // 1. Context length check
    // -----------------------
    const auto n_ctx = llama_n_ctx(model->impl_->ctx);
    if (model->impl_->n_past + max_tokens > n_ctx) {
        LOG(WARNING) << "Context overflow likely. Resetting KV cache.";
        // llama_kv_cache_clear(model->impl_->ctx);     // <-- correct way
        model->impl_->n_past = 0;
    }

    // -----------------------
    // 2. Construct Chat Prompt (Mistral format)
    // -----------------------
    std::string system = SYSTEM_PROMPT;

    std::string formatted_prompt = 
        "<s>[INST] " + 
        system + 
        "\n" + 
        std::string(prompt) + 
        " [/INST]";

    LOG(INFO) << "Formatted prompt: " << std::quoted(formatted_prompt);

    // -----------------------
    // 3. Tokenize (do NOT add BOS manually)
    // -----------------------
    std::vector<llama_token> tokens =
        tokenize(model->impl_->model, formatted_prompt);

    if (tokens.empty()) {
        LOG(ERROR) << "Tokenization failed for prompt: " << prompt;
        return std::nullopt;
    }

    LOG(INFO) << "Tokenized prompt into " << tokens.size() << " tokens.";

    // -----------------------
    // 4. Load prompt tokens
    // -----------------------
    llama_batch_clear(model->impl_->batch);
    for (size_t i = 0; i < tokens.size(); i++) {
        llama_batch_add(
            model->impl_->batch,
            tokens[i],
            model->impl_->n_past + i,
            {model->impl_->seq_id},
            (i == tokens.size() - 1)
        );
    }

    // Decode
    if (llama_decode(model->impl_->ctx, model->impl_->batch) != 0) {
        LOG(ERROR) << "Decode failed for prompt";
        return std::nullopt;
    }

    model->impl_->n_past += model->impl_->batch.n_tokens;

    // -----------------------
    // 5. Generation Loop
    // -----------------------
    std::stringstream response_stream;

    for (int i = 0; i < max_tokens; i++) {

        llama_token tok = llama_sampler_sample(
            model->impl_->sampler,
            model->impl_->ctx,
            -1
        );

        const llama_vocab* vocab = llama_model_get_vocab(model->impl_->model);

        if (llama_vocab_is_eog(vocab, tok)) {
            LOG(INFO) << "EoG token generated, stopping.";
            break;
        }

        // decode piece
        std::array<char, 4096> buf{};
        int n = llama_token_to_piece(vocab, tok, buf.data(), buf.size()-1, 0, true);

        if (n > 0) {
            response_stream.write(buf.data(), n);
        }

        // prepare for next step
        llama_batch_clear(model->impl_->batch);
        llama_batch_add(model->impl_->batch, tok, model->impl_->n_past,
                        {model->impl_->seq_id}, true);

        if (llama_decode(model->impl_->ctx, model->impl_->batch) != 0) {
            LOG(ERROR) << "Decode failed during generation";
            break;
        }

        model->impl_->n_past += 1;
    }

    std::string response_text = response_stream.str();
    LOG(INFO) << "Raw response: " << response_text;

    // -----------------------
    // 6. Extract <|start|>assistant<|channel|>final<|message|>...
    // .
    // -----------------------
    std::string thought, answer;

    constexpr std::string_view thought_tag_start = "<|start|>assistant<|channel|>final<|message|>";
    auto pos = response_text.rfind(thought_tag_start);
    if (pos != std::string::npos) {
        answer = response_text.substr(pos + thought_tag_start.length());
    } else {
        // Fallback: treat entire response as answer
        thought = "";
        answer = response_text;
    }

    // -----------------------
    // 7. Return
    // -----------------------
    auto end_time = std::chrono::steady_clock::now();

    Model::Response r;
    r.thought   = std::move(thought);
    r.answer    = std::move(answer);
    r.duration  = end_time - start_time;

    return r;
}