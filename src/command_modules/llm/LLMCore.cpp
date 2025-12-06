#include "LLMCore.hpp"

#include <absl/log/log.h>

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

#include "llama.h"

struct LLMCore::Model::Impl {
    llama_model* model = nullptr;
    llama_context* ctx = nullptr;
    llama_sampler* sampler = nullptr;
    llama_batch batch{};
    int n_past = 0;
};

LLMCore::Model::Model() { impl_ = std::make_unique<Impl>(); }
LLMCore::Model::~Model() {}

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
    if (!impl_->model) {
        LOG(ERROR) << "Failed to load model from " << modelPath.string();
        return false;
    }
    impl_->ctx = llama_init_from_model(impl_->model, ctx_params);
    if (!impl_->ctx) {
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
        if (len < 0) return "N/A";
        std::string buffer(len + 1, '\0');
        llama_model_meta_val_str(model, key, &buffer[0], len + 1);
        return buffer;
    };

    // Fill in metadata
    info.name = get_meta("general.name");
    info.architecture = get_meta("general.architecture");
    info.description = get_meta("general.description");
    return info;
}

void LLMCore::Model::cleanup() {
    if (impl_->ctx) {
        llama_batch_free(impl_->batch);
        llama_sampler_free(impl_->sampler);
        llama_free(impl_->ctx);
        impl_->ctx = nullptr;
    }
    if (impl_->model) {
        llama_model_free(impl_->model);
        impl_->model = nullptr;
    }
}

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

LLMCore::LLMCore() { 
    DLOG(INFO) << "LLMCore Constructor called";
    llama_backend_init();
}

LLMCore::~LLMCore() { 
    DLOG(INFO) << "LLMCore Destructor called";
    llama_backend_free();
}

std::optional<LLMCore::Model::Response> LLMCore::query(const Model* model, const std::string_view prompt, int max_tokens) {
    auto start_time = std::chrono::steady_clock::now();
    LOG(INFO) << "LLMCore::query called with prompt: " << prompt;

    // 1. Define the messages (System + User)
    std::vector<llama_chat_message> messages;
    // System Message
    messages.push_back(
        {"system",
         "You are a helpful Telegram chatbot named Miku. Always start with "
         "'Hello, I am your Miku!' and reply with English only. Use ```code``` format for codes."});

    // User Message
    std::string user_prompt = std::string(prompt);
    messages.push_back({"user", user_prompt.c_str()});

    // 2. Calculate required buffer size for the formatted template
    // We pass nullptr first to get the length
    int new_len = llama_chat_apply_template(
        R"([SYSTEM_PROMPT]{{system_message}}[/SYSTEM_PROMPT]
[INST]{{prompt}}[/INST]
{{assistant_message}})",        // template
        messages.data(),  // messages
        messages.size(),  // number of messages
        true,             // add_assistent_prefix (trigger generation)
        nullptr,          // output buffer
        0                 // buffer size
    );

    if (new_len < 0) {
        LOG(ERROR) << "Failed to apply chat template";
        return std::nullopt;
    }
    // 3. Allocate buffer and apply template again to fill it
    std::string formatted_prompt(new_len + 1, '\0');
    llama_chat_apply_template(
        nullptr,          // template (nullptr = use model default)
        messages.data(),  // messages
        messages.size(),  // number of messages
        true
        ,                // add_assistent_prefix (trigger generation)
        &formatted_prompt[0],  // output buffer
        formatted_prompt.size()  // buffer size
    );

    LOG(INFO) << "Formatted prompt: " << std::quoted(formatted_prompt);

    // Tokenize Prompt
    std::vector<llama_token> tokens =
        tokenize(model->impl_->model, formatted_prompt);

    if (tokens.empty()) {
        LOG(ERROR) << "Tokenization failed for prompt: " << prompt;
        return std::nullopt;
    }

    LOG(INFO) << "Tokenized prompt into " << tokens.size() << " tokens.";


    // Load Prompt
    for (size_t i = 0; i < tokens.size(); i++) {
        llama_batch_add(model->impl_->batch, tokens[i], i, {0},
                        (i == tokens.size() - 1));
    }

    // Decode Prompt
    if (llama_decode(model->impl_->ctx, model->impl_->batch) != 0) {
        LOG(ERROR) << "Decode failed for prompt";
        return std::nullopt;
    }
    model->impl_->n_past += model->impl_->batch.n_tokens;  // Update n_past

    // Generate Response
    // --- MAIN LOOP ---
    std::stringstream response_stream;

    for (int i = 0; i < max_tokens; i++) {
        // Sample next token
        llama_token new_token_id =
            llama_sampler_sample(model->impl_->sampler, model->impl_->ctx, -1);
        const llama_vocab* vocab = llama_model_get_vocab(model->impl_->model);

        // Check for End of Text
        if (llama_vocab_is_eog(vocab, new_token_id)) {
            DLOG(INFO) << "EoG token generated, stopping.";
            break;
        }

        // Print token
        std::array<char, 256> buf{};
        int n = llama_token_to_piece(vocab, new_token_id, buf.data(),
                                     buf.size() - 1, /*lstrip*/ 0,
                                     /*special*/ true);
        if (n >= 0) {
            DLOG(INFO) << "Generated token: " << std::string(buf.data(), n);
            response_stream.write(buf.data(), n);
        }

        // Prepare next batch (1 token)
        llama_batch_clear(model->impl_->batch);
        llama_batch_add(model->impl_->batch, new_token_id, model->impl_->n_past,
                        {0}, true);

        // Decode
        if (llama_decode(model->impl_->ctx, model->impl_->batch) != 0) {
            LOG(ERROR) << "Decode failed during generation.";
            break;
        }
        model->impl_->n_past += 1;
    }

    LOG(INFO) << "Generation completed.";

    // Do the spliting of thought and answer here
    std::string response_text = response_stream.str();
    // Format is <think>Thought: ... </think>Answer. So we split by "</think>"
    size_t thought_end = response_text.find("</think>");
    std::string thought, answer;
    if (thought_end != std::string::npos) {
        thought = response_text.substr(0, thought_end);
        answer = response_text.substr(thought_end + 8);  // 8 = length
    } else {
        thought = response_text;
        answer = "N/A";
    }

    auto end_time = std::chrono::steady_clock::now();
    Model::Response response;
    response.thought = std::move(thought);
    response.answer = std::move(answer);
    response.duration = end_time - start_time;
    return response;
}
