#include "LLMCore.hpp"

#include <absl/log/log.h>

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "SYSTEM_PROMPT.hpp"
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
    model_params.use_mmap = true;
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
    auto sparams = llama_sampler_chain_default_params();
    impl_->sampler = llama_sampler_chain_init(sparams);

    // 1. Add Repetition Penalties (Prevents "We... We... We...")
    //    penalty_last_n: 64 (look back 64 tokens)
    //    penalty_repeat: 1.1 (punish repeats)
    //    penalty_freq:   0.0
    //    penalty_present: 0.0
    llama_sampler_chain_add(impl_->sampler,
                            llama_sampler_init_penalties(64, 1.1f, 0.0f, 0.0f));

    // 2. Add Top-K (Limits vocabulary to top K probable tokens)
    llama_sampler_chain_add(impl_->sampler, llama_sampler_init_top_k(40));

    // 3. Add Top-P (Nucleus sampling)
    llama_sampler_chain_add(impl_->sampler, llama_sampler_init_top_p(0.95f, 1));

    // 4. Add Temperature (Controls creativity/randomness)
    //    seed: -1 (random) or specific number for reproducibility
    llama_sampler_chain_add(impl_->sampler, llama_sampler_init_dist(1234));
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
        model->impl_->n_past = 0;
    }

    // -----------------------
    // 2. Construct Chat Prompt (Mistral format)
    // -----------------------
    std::string system = SYSTEM_PROMPT;

    // The Harmony format structure:
    // <|start|>system<|message|> ... <|end|>
    // <|start|>user<|message|> ... <|end|>
    // <|start|>assistant<|channel|>
    std::string formatted_prompt = "<|start|>system<|message|>" + system +
                                   "<|end|>" + "<|start|>user<|message|>" +
                                   std::string(prompt) + "<|end|>" +
                                   "<|start|>assistant<|channel|>";

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
        llama_batch_add(model->impl_->batch, tokens[i],
                        model->impl_->n_past + i, {model->impl_->seq_id},
                        (i == tokens.size() - 1));
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
        llama_token tok =
            llama_sampler_sample(model->impl_->sampler, model->impl_->ctx, -1);

        llama_sampler_accept(model->impl_->sampler, tok);

        const llama_vocab* vocab = llama_model_get_vocab(model->impl_->model);

        if (llama_vocab_is_eog(vocab, tok)) {
            LOG(INFO) << "EoG token generated, stopping.";
            break;
        }

        // decode piece
        std::array<char, 4096> buf{};
        int n = llama_token_to_piece(vocab, tok, buf.data(), buf.size() - 1, 0,
                                     true);

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
    // 6. Extract Harmony Format (Reasoning vs Final)
    // -----------------------
    std::string thought, answer;

    // The marker that splits Thought from Answer
    // Note: We search for "final<|message|>" because that's the specific tag
    // this architecture uses to switch modes.
    constexpr std::string_view split_marker =
        "<|end|><|start|>assistant<|channel|>final<|message|>";

    auto pos = response_text.find(split_marker);

    if (pos != std::string::npos) {
        // FOUND: The model finished thinking and gave an answer.

        // 1. Extract Thought (everything before the marker)
        thought = response_text.substr(0, pos);

        // 2. Extract Answer (everything after the marker)
        answer = response_text.substr(pos + split_marker.length());

        // Optional: Clean up the "analysis<|message|>" tag from the start of
        // thought
        std::string_view analysis_start = "analysis<|message|>";
        if (thought.find(analysis_start) == 0) {
            thought.erase(0, analysis_start.length());
        }

    } else {
        // NOT FOUND: The model stopped early (hit max_tokens) or failed.

        // In this case, we assume everything generated so far is just
        // "Thought".
        thought = response_text;
        answer =
            "[Error: Model hit max_tokens limit while thinking. Increase "
            "max_tokens.]";
    }

    // -----------------------
    // 7. Return
    // -----------------------
    auto end_time = std::chrono::steady_clock::now();

    Model::Response r;
    r.thought = std::move(thought);
    r.answer = std::move(answer);
    r.duration = end_time - start_time;

    return r;
}