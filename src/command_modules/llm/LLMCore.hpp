#pragma once

#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

class LLMCore {
    struct Context;

   public:
    LLMCore();
    ~LLMCore();

    class Model {
       public:
        friend class LLMCore;

        /*
         * Model Information Structure
         */
        struct Info {
            std::string name;          // Model name
            std::string architecture;  // Model architecture
            std::string description;   // Model description
            uint64_t parameterCount;   // Number of parameters
            uint64_t fileSizeBytes;    // Size of model file in bytes
        };

        /*
         * Model Response Structure
         */
        struct Response {
            std::string thought;
            std::string answer;
            std::chrono::steady_clock::duration duration;
        };

        /*
         * Constructor and Destructor
         */
        Model();
        ~Model();

        /*
         * Load model from file
         * 
         * #modelPath: Path to the model file
         * #n_gpu_layers: Number of layers to offload to GPU (default: 99)
         * #n_ctx: Context size (default: 16384)
         * #n_batch: Batch size (default: 2048)
         * 
         * Returns true on success, false on failure
         */
        bool load(const std::filesystem::path& modelPath, int n_gpu_layers = 99,
                  int n_ctx = 16384, int n_batch = 2048);

        /*
         * Get model information
         * Returns an Info struct containing model metadata
         */
        Info info() const;

        /*
         * Cleanup model resources
         */
        void cleanup();

       private:
        struct Impl;
        std::unique_ptr<Impl> impl_;
    };

    /*
     * Query the model with a prompt
     * 
     * #model: The model struct
     * #prompt: The input prompt string
     * #max_tokens: Max token used. Default 4096.
     * 
     * Returns an optional Response struct containing the model's response
     */
    std::optional<Model::Response> query(const Model* model, const std::string_view prompt, int max_tokens = 4096);
};