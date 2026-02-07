#include <absl/log/log.h>
#include <absl/strings/str_replace.h>
#include <absl/strings/str_split.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <tgbot/TgException.h>

#include <algorithm>
#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <api/TgBotApi.hpp>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string_view>

#include "CurlUtils.hpp"
#include "llm/LMStudioApi.hpp"
#include "llm/SYSTEM_PROMPT.hpp"
#include "utils/ConfigManager.hpp"

#ifdef ASK_ENABLE_LOCAL_LLM
#include "llm/LLMCore.hpp"

constexpr const char* kLLMStorageKey = "LLMCoreInstance";

struct LLMCoreInstance {
    LLMCore llm;
    LLMCore::Model model;
};

// Handler for local model LLM <i.e. With supported GPU>
static void localmodelhandler(TgBotApi::Ptr api, MessageExt* message,
                              const StringResLoader::PerLocaleMap* res,
                              const Providers* provider,
                              const std::filesystem::path& modelPath) {
    LLMCoreInstance* instance = nullptr;

    auto sent = api->sendReplyMessage(message->message(),
                                      "Processing your query, please wait...");

    if (provider->globalStorage->hasStorage(kLLMStorageKey)) {
        instance = provider->globalStorage->getStorage(kLLMStorageKey)
                       .getAs<LLMCoreInstance>();

        if (!message->has<MessageAttrs::ExtraText>()) {
            auto info = instance->model.info();
            api->editMessage(
                sent,
                fmt::format(R"(Model Information:
Name: {}
Architecture: {}
Description: {}
Parameter Count: {}
File Size: {} bytes)",
                            info.name, info.architecture, info.description,
                            info.parameterCount, info.fileSizeBytes));
            return;
        }
    } else {
        provider->globalStorage->getStorage(kLLMStorageKey) =
            MakeSharedMallocFrom<LLMCoreInstance>();

        instance = provider->globalStorage->getStorage(kLLMStorageKey)
                       .getAs<LLMCoreInstance>();

        api->editMessage(sent, "Initializing LLM core, please wait...");
        if (!instance->model.load(modelPath)) {
            LOG(ERROR) << "Failed to initialize LLM core.";
            api->editMessage(sent, "Error: Unable to initialize LLM core.");
            provider->globalStorage->removeStorage(kLLMStorageKey);
            return;
        }
    }

    if (!message->has<MessageAttrs::ExtraText>()) {
        api->editMessage(
            sent, "Please provide a query after the command to ask the LLM.");
        return;
    }

    std::string query = message->get<MessageAttrs::ExtraText>();
    auto response_opt = instance->llm.query(&instance->model, query, 512);
    if (!response_opt) {
        LOG(ERROR) << "LLM query failed.";
        api->editMessage(sent, "Error: LLM query failed.");
        return;
    }
    const auto& response = *response_opt;
    std::initializer_list<std::pair<absl::string_view, absl::string_view>>
        replacements = {{"`", "\\`"}, {"_", "\\_"}, {"*", "\\*"}, {"[", "\\["},
                        {"]", "\\]"}, {"(", "\\("}, {")", "\\)"}, {"~", "\\~"},
                        {"`", "\\`"}, {">", "\\>"}, {"#", "\\#"}, {"+", "\\+"},
                        {"-", "\\-"}, {"=", "\\="}, {"|", "\\|"}, {"{", "\\{"},
                        {"}", "\\}"}, {".", "\\."}, {"!", "\\!"}};
    std::string response_thought =
        absl::StrReplaceAll(response.thought, replacements);
    std::string response_answer =
        absl::StrReplaceAll(response.answer, replacements);

    std::string reply = fmt::format("Thought: {}", std::move(response_thought));
    api->editMessage(sent, reply);
    api->sendMessage(message->get<MessageAttrs::Chat>(),
                     fmt::format("Answer: {}", std::move(response_answer)));
    api->sendMessage(
        message->get<MessageAttrs::Chat>(),
        fmt::format("Query processed in {:%S} seconds.", response.duration));
}
#endif  // ASK_ENABLE_LOCAL_LLM

constexpr float kTemperature = 0.7F;

static void localnetmodelhandler(TgBotApi::Ptr api, MessageExt* message,
                                 const StringResLoader::PerLocaleMap* res,
                                 const Providers* provider,
                                 const std::string_view url,
                                 const std::string_view authkey = "") {
    auto sent = api->sendReplyMessage(message->message(),
                                      "Processing your query, please wait...");
    if (!message->has<MessageAttrs::ExtraText>()) {
        api->editMessage(
            sent, "Please provide a query after the command to ask the LLM.");
        return;
    }

    std::string query = message->get<MessageAttrs::ExtraText>();

    // GET /api/models to check server availability
    std::optional<std::string> modelsResponse;
    if (modelsResponse = CurlUtils::download_memory(
            std::string(url) + LMStudioApi::kModelsEndpoint, nullptr, authkey);
        !modelsResponse) {
        LOG(ERROR) << "Failed to connect to LLM server at " << url;
        api->editMessage(sent,
                         "Error: Failed to connect to LLM server at the "
                         "configured URL.");
        return;
    }
    LMStudioApi::ModelResponse modelResponse;
    try {
        modelResponse = nlohmann::json::parse(*modelsResponse)
                            .get<LMStudioApi::ModelResponse>();
    } catch (const std::exception& e) {
        LOG(ERROR) << "Failed to parse models response from LLM server: "
                   << e.what();
        LOG(ERROR) << "Response content: " << *modelsResponse;
        api->editMessage(
            sent, "Error: Failed to parse models response from LLM server.");
        return;
    }
    LOG(INFO) << "LLM Server Models Available:";
    for (const auto& model : modelResponse.models) {
        LOG(INFO) << " - " << model.key << " (owned by " << model.publisher
                  << ")";
    }
    if (modelResponse.models.empty()) {
        LOG(ERROR) << "No models available from LLM server.";
        api->editMessage(sent, "Error: No models available from LLM server.");
        return;
    }

    // TODO: Extraction logic, for now use index 0
    LMStudioApi::ChatRequest chatRequest;
    auto that = std::ranges::find_if(
        modelResponse.models, [](const LMStudioApi::Model& m) {
            return m.type == LMStudioApi::LLMType::llm;
        });

    static std::string response_key;
    if (that == modelResponse.models.end()) {
        LOG(ERROR) << "No LLM type models available from LLM server.";
        api->editMessage(
            sent, "Error: No LLM type models available from LLM server.");
        return;
    }
    chatRequest.model = that->key;
    LOG(INFO) << "Using model " << chatRequest.model << " for query.";
    chatRequest.temperature = 0.2f;
    chatRequest.system_prompt = SYSTEM_PROMPT;
    chatRequest.input = query;
    std::vector<LMStudioApi::ChatRequest::Plugin> plugins{};
    plugins.emplace_back(LMStudioApi::ChatRequest::Plugin{
        .type = "plugin",
        .id = "mcp/playwright",
    });
    chatRequest.integrations = plugins;
    chatRequest.context_length = 64000;
    chatRequest.reasoning = LMStudioApi::Reasoning::medium;
    if (!response_key.empty()) {
        chatRequest.previous_response_id = response_key;
    }

    nlohmann::json payload = chatRequest;

    if (auto rc = CurlUtils::send_json_get_reply(
            std::string(url) + LMStudioApi::kChatEndpoint, payload.dump(),
            authkey);
        rc) {
        try {
            LMStudioApi::ChatResponse chatResponse =
                nlohmann::json::parse(*rc).get<LMStudioApi::ChatResponse>();
            if (chatResponse.output.empty()) {
                LOG(ERROR) << "LLM server returned empty outputs.";
                LOG(ERROR) << "Response content: " << *rc;
                api->editMessage(
                    sent,
                    "Error: LLM server returned empty outputs in response.");
                return;
            }
            auto it = std::ranges::find_if(
                chatResponse.output,
                [](const LMStudioApi::ChatResponse::Output& output) {
                    return output.type == "message";
                });
            if (it == chatResponse.output.end()) {
                LOG(ERROR)
                    << "LLM server response has no 'message' type output.";
                LOG(ERROR) << "Response content: " << *rc;
                api->editMessage(
                    sent,
                    "Error: LLM server response has no 'message' type output.");
                return;
            }
            response_key = chatResponse.response_id.value_or("");
            api->editMessage<TgBotApi::ParseMode::MarkdownV2>(sent,
                                                              it->content);
        } catch (const std::exception& e) {
            LOG(ERROR) << "Failed to parse LLM server response: " << e.what();
            api->editMessage(sent,
                             "Error: Failed to parse LLM server response.");
        }
    } else {
        LOG(ERROR) << "Failed to get response from LLM server.";
        api->editMessage(sent,
                         "Error: Failed to get response from LLM server.");
    }
}

DECLARE_COMMAND_HANDLER(ask) {
    auto mgr = provider->config.get();

    if (!mgr->get(ConfigManager::Configs::LLM_TYPE) ||
        !mgr->get(ConfigManager::Configs::LLM_LOCATION)) {
        api->sendMessage(
            message->get<MessageAttrs::Chat>(),
            "LLM functionality is not configured. Please set up the LLM "
            "configuration first.");
        return;
    }

    std::string type = *mgr->get(ConfigManager::Configs::LLM_TYPE);
    std::string location = *mgr->get(ConfigManager::Configs::LLM_LOCATION);
    auto authkey = mgr->get(ConfigManager::Configs::LLM_AUTHKEY);

    LOG(INFO) << "LLM Configuration - Type: " << type
              << ", Path/URL: " << location;

    if (type == "local") {
#ifdef ASK_ENABLE_LOCAL_LLM
        localmodelhandler(api, message, res, provider, location);
#else
        LOG(ERROR) << "Local LLM support is not enabled in this build.";
#endif
    } else if (type == "localnet") {
        if (authkey) {
            LOG(INFO) << "Using authkey for localnet LLM.";
            localnetmodelhandler(api, message, res, provider, location,
                                 *authkey);
        } else {
            localnetmodelhandler(api, message, res, provider, location);
        }
    } else {
        LOG(ERROR) << "Unsupported LLM configuration type: " << type;
    }
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "ask",
    .description = "Ask a query to an LLM",
    .function = COMMAND_HANDLER_NAME(ask),
};
