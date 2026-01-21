#include <absl/log/log.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <tgbot/TgException.h>

#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <api/TgBotApi.hpp>
#include <filesystem>
#include <nlohmann/json.hpp>

#include "CurlUtils.hpp"
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
                                 const std::string_view url) {
    auto sent = api->sendReplyMessage(message->message(),
                                      "Processing your query, please wait...");
    if (!message->has<MessageAttrs::ExtraText>()) {
        api->editMessage(
            sent, "Please provide a query after the command to ask the LLM.");
        return;
    }

    std::string query = message->get<MessageAttrs::ExtraText>();

    nlohmann::json payload = {
        {"model", "local-model"},
        {"messages",
         {{{"role", "system"}, {"content", SYSTEM_PROMPT}},
          {{"role", "user"}, {"content", query}}}},
        {"temperature", kTemperature}};

    if (auto rc = CurlUtils::send_json_get_reply(url, payload.dump()); rc) {
        try {
            auto j = nlohmann::json::parse(*rc);
            if (j.contains("choices") && j["choices"].is_array() &&
                !j["choices"].empty()) {
                std::string answer =
                    j["choices"][0]["message"]["content"].get<std::string>();
                api->editMessage(sent, fmt::format("Answer: {}", answer));
            } else {
                LOG(ERROR) << "Invalid response format from LLM server: "
                           << *rc;
                api->editMessage(
                    sent, "Error: Invalid response format from LLM server.");
            }
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

    std::pair<std::string, std::string> parsedConfig;
    size_t delimiterPos = config.find(':');
    if (delimiterPos != std::string::npos) {
        parsedConfig.first = config.substr(0, delimiterPos);
        parsedConfig.second = config.substr(delimiterPos + 1);
    } else {
        LOG(ERROR)
            << "Invalid LLM configuration format: Cannot find delimiter: "
            << config;
        return;
    }

    LOG(INFO) << "LLM Configuration - Type: " << parsedConfig.first
              << ", Path/URL: " << parsedConfig.second;

    if (parsedConfig.first == "local") {
#ifdef ASK_ENABLE_LOCAL_LLM
        localmodelhandler(api, message, res, provider, parsedConfig.second);
#else
        LOG(ERROR) << "Local LLM support is not enabled in this build.";
#endif
    } else if (parsedConfig.first == "localnet") {
        localnetmodelhandler(api, message, res, provider, parsedConfig.second);
    } else {
        LOG(ERROR) << "Unsupported LLM configuration type: "
                   << parsedConfig.first;
    }
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "ask",
    .description = "Ask a query to an LLM",
    .function = COMMAND_HANDLER_NAME(ask),
};
