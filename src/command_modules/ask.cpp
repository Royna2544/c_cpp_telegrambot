#include <absl/log/log.h>
#include <absl/strings/str_split.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <tgbot/TgException.h>

#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <api/TgBotApi.hpp>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <string_view>

#include "CurlUtils.hpp"
#include "llm/OpenAI_api.hpp"
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
    std::string reply = fmt::format(R"(
Thought: {}
)",
                                    response.thought);
    api->editMessage(sent, reply);
    api->sendMessage(message->get<MessageAttrs::Chat>(),
                     fmt::format("Answer: {}", response.answer));
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
            std::string(url) + openai::kOpenAI_API_Models_Endpoint, nullptr,
            authkey);
        !modelsResponse) {
        LOG(ERROR) << "Failed to connect to LLM server at " << url;
        api->editMessage(sent,
                         "Error: Failed to connect to LLM server at the "
                         "configured URL.");
        return;
    }
    openai::ModelResponse modelResponse;
    try {
        modelResponse =
            nlohmann::json::parse(*modelsResponse).get<openai::ModelResponse>();
    } catch (const std::exception& e) {
        LOG(ERROR) << "Failed to parse models response from LLM server: "
                   << e.what();
        api->editMessage(
            sent, "Error: Failed to parse models response from LLM server.");
        return;
    }
    LOG(INFO) << "LLM Server Models Available:";
    for (const auto& model : modelResponse.data) {
        LOG(INFO) << " - " << model.id << " (owned by " << model.owned_by
                  << ")";
    }
    if (modelResponse.data.empty()) {
        LOG(ERROR) << "No models available from LLM server.";
        api->editMessage(sent, "Error: No models available from LLM server.");
        return;
    }

    // TODO: Extraction logic, for now use index 0
    openai::ChatRequest chatRequest;
    chatRequest.model = modelResponse.data[0].id;
    LOG(INFO) << "Using model " << chatRequest.model << " for query.";
    chatRequest.temperature = kTemperature;
    chatRequest.messages = {openai::Message::System(SYSTEM_PROMPT),
                            openai::Message::User(query)};

    nlohmann::json payload = chatRequest;

    if (auto rc = CurlUtils::send_json_get_reply(
            std::string(url) + openai::kOpenAI_API_ChatCompletions_Endpoint,
            payload.dump(), authkey);
        rc) {
        try {
            openai::ChatResponse chatResponse =
                nlohmann::json::parse(*rc).get<openai::ChatResponse>();
            if (chatResponse.choices.empty()) {
                LOG(ERROR) << "LLM server returned empty choices.";
                api->editMessage(
                    sent,
                    "Error: LLM server returned empty choices in response.");
                return;
            }
            api->editMessage(sent, chatResponse.choices[0].message.content);
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

    if (!mgr->get(ConfigManager::Configs::LLMCONFIG)) {
        api->sendMessage(
            message->get<MessageAttrs::Chat>(),
            "LLM functionality is not configured. Please set up the LLM "
            "configuration first.");
        return;
    }

    std::string config = *mgr->get(ConfigManager::Configs::LLMCONFIG);

    std::vector<std::string> parsedConfig;
    parsedConfig = absl::StrSplit(config, ',');
    if (parsedConfig.size() < 2 || parsedConfig.size() > 3) {
        LOG(ERROR) << "Invalid LLM configuration format: " << config;
    }

    LOG(INFO) << "LLM Configuration - Type: " << parsedConfig[0]
              << ", Path/URL: " << parsedConfig[1];

    if (parsedConfig[0] == "local") {
#ifdef ASK_ENABLE_LOCAL_LLM
        localmodelhandler(api, message, res, provider, parsedConfig[1]);
#else
        LOG(ERROR) << "Local LLM support is not enabled in this build.";
#endif
    } else if (parsedConfig[0] == "localnet") {
        if (parsedConfig.size() == 3) {
            LOG(INFO) << "Using authkey for localnet LLM.";
            localnetmodelhandler(api, message, res, provider, parsedConfig[1],
                                 parsedConfig[2]);
        } else {
            localnetmodelhandler(api, message, res, provider, parsedConfig[1]);
        }
    } else {
        LOG(ERROR) << "Unsupported LLM configuration type: " << parsedConfig[0];
    }
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "ask",
    .description = "Ask a query to an LLM",
    .function = COMMAND_HANDLER_NAME(ask),
};
