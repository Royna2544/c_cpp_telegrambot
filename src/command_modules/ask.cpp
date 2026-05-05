#include <absl/log/log.h>
#include <absl/strings/str_replace.h>
#include <absl/strings/str_split.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <tgbot/TgException.h>

#include <algorithm>
#include <cstdint>
#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <api/TgBotApi.hpp>
#include <filesystem>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string_view>
#include <unordered_map>

#include "CurlUtils.hpp"
#include "llm/LMStudioApi.hpp"
#include "llm/SYSTEM_PROMPT.hpp"
#include "utils/ConfigManager.hpp"

#ifdef ASK_ENABLE_LOCAL_LLM
#include "llm/LLMCore.hpp"

struct LLMCoreInstance {
    LLMCore llm;
    LLMCore::Model model;
    std::filesystem::path modelPath;
};

// Handler for local model LLM <i.e. With supported GPU>
static void localmodelhandler(TgBotApi::Ptr api, MessageExt* message,
                              const StringResLoader::PerLocaleMap* res,
                              const std::filesystem::path& modelPath) {
    static std::mutex llm_mutex;
    static std::unique_ptr<LLMCoreInstance> instance;

    auto sent = api->sendReplyMessage(message->message(),
                                      res->get(Strings::LLM_PROCESSING_QUERY));

    std::unique_lock<std::mutex> lock(llm_mutex, std::defer_lock);
    if (!lock.try_lock()) {
        api->editMessage(sent, res->get(Strings::LLM_BUSY_WAIT));
        lock.lock();
    }

    if (instance != nullptr && instance->modelPath == modelPath) {
        if (!message->has<MessageAttrs::ExtraText>()) {
            auto info = instance->model.info();
            api->editMessage(
                sent,
                fmt::format(fmt::runtime(res->get(Strings::LLM_MODEL_INFORMATION)),
                            info.name, info.architecture, info.description,
                            info.parameterCount, info.fileSizeBytes));
            return;
        }
    } else {
        if (!message->has<MessageAttrs::ExtraText>()) {
            api->editMessage(sent, res->get(Strings::LLM_PROVIDE_QUERY));
            return;
        }

        api->editMessage(sent, res->get(Strings::LLM_INITIALIZING_CORE));
        instance.reset();
        auto new_instance = std::make_unique<LLMCoreInstance>();
        if (!new_instance->model.load(modelPath)) {
            LOG(ERROR) << "Failed to initialize LLM core.";
            api->editMessage(sent, res->get(Strings::LLM_INIT_FAILED));
            return;
        }
        new_instance->modelPath = modelPath;
        instance = std::move(new_instance);
    }

    if (!message->has<MessageAttrs::ExtraText>()) {
        api->editMessage(sent, res->get(Strings::LLM_PROVIDE_QUERY));
        return;
    }

    std::string query = message->get<MessageAttrs::ExtraText>();
    LLMCore::QueryError query_error = LLMCore::QueryError::None;
    auto response_opt =
        instance->llm.query(&instance->model, query, 512, &query_error);
    if (!response_opt) {
        LOG(ERROR) << "LLM query failed.";
        if (query_error == LLMCore::QueryError::PromptTooLong) {
            api->editMessage(sent, res->get(Strings::LLM_PROMPT_TOO_LONG));
            return;
        }
        api->editMessage(sent, res->get(Strings::LLM_QUERY_FAILED));
        return;
    }
    const auto& response = *response_opt;
    std::initializer_list<std::pair<absl::string_view, absl::string_view>>
        replacements = {{"`", ""},    {"_", "\\_"}, {"*", "\\*"}, {"[", "\\["},
                        {"]", "\\]"}, {"(", "\\("}, {")", "\\)"}, {"~", "\\~"},
                        {"`", "\\`"}, {">", "\\>"}, {"#", "\\#"}, {"+", "\\+"},
                        {"-", "\\-"}, {"=", "\\="}, {"|", "\\|"}, {"{", "\\{"},
                        {"}", "\\}"}, {".", "\\."}, {"!", "\\!"}};
    std::string response_thought =
        absl::StrReplaceAll(response.thought, replacements);
    std::string response_answer =
        absl::StrReplaceAll(response.answer, replacements);

    std::string reply =
        fmt::format(fmt::runtime(res->get(Strings::LLM_THOUGHT_PREFIX)),
                    std::move(response_thought));
    api->editMessage(sent, reply);
    api->sendMessage(message->get<MessageAttrs::Chat>(),
                     fmt::format(fmt::runtime(res->get(Strings::LLM_ANSWER_PREFIX)),
                                 std::move(response_answer)));
    api->sendMessage(
        message->get<MessageAttrs::Chat>(),
        fmt::format(fmt::runtime(res->get(Strings::LLM_QUERY_PROCESSED)),
                    response.duration));
}
#endif  // ASK_ENABLE_LOCAL_LLM

static void localnetmodelhandler(TgBotApi::Ptr api, MessageExt* message,
                                 const StringResLoader::PerLocaleMap* res,
                                 const Providers* provider,
                                 const std::string_view url,
                                 const std::string_view authkey = "") {
    auto sent = api->sendReplyMessage(message->message(),
                                      res->get(Strings::LLM_PROCESSING_QUERY));
    if (!message->has<MessageAttrs::ExtraText>()) {
        api->editMessage(sent, res->get(Strings::LLM_PROVIDE_QUERY));
        return;
    }

    std::string query = message->get<MessageAttrs::ExtraText>();

    // GET /api/models to check server availability
    std::optional<std::string> modelsResponse;
    if (modelsResponse = CurlUtils::download_memory(
            std::string(url) + LMStudioApi::kModelsEndpoint, nullptr, authkey);
        !modelsResponse) {
        LOG(ERROR) << "Failed to connect to LLM server at " << url;
        api->editMessage(sent, res->get(Strings::LLM_CONNECT_FAILED));
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
        api->editMessage(sent, res->get(Strings::LLM_PARSE_MODELS_FAILED));
        return;
    }
    LOG(INFO) << "LLM Server Models Available:";
    for (const auto& model : modelResponse.models) {
        LOG(INFO) << " - " << model.key << " (owned by " << model.publisher
                  << ")";
    }
    if (modelResponse.models.empty()) {
        LOG(ERROR) << "No models available from LLM server.";
        api->editMessage(sent, res->get(Strings::LLM_NO_MODELS));
        return;
    }

    // TODO: Extraction logic, for now use index 0
    LMStudioApi::ChatRequest chatRequest;
    auto that = std::ranges::find_if(
        modelResponse.models, [](const LMStudioApi::Model& m) {
            return m.type == LMStudioApi::LLMType::llm;
        });

    if (that == modelResponse.models.end()) {
        LOG(ERROR) << "No LLM type models available from LLM server.";
        api->editMessage(sent, res->get(Strings::LLM_NO_LLM_MODELS));
        return;
    }
    chatRequest.model = that->key;
    LOG(INFO) << "Using model " << chatRequest.model << " for query.";
    chatRequest.system_prompt = SYSTEM_PROMPT;
    chatRequest.input = query;

    const std::int64_t chat_id = message->get<MessageAttrs::Chat>()->id;
    static std::mutex response_keys_mutex;
    static std::unordered_map<std::int64_t, std::string> response_keys;
    {
        std::lock_guard<std::mutex> lock(response_keys_mutex);
        if (auto it = response_keys.find(chat_id);
            it != response_keys.end() && !it->second.empty()) {
            chatRequest.previous_response_id = it->second;
        }
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
                api->editMessage(sent, res->get(Strings::LLM_EMPTY_OUTPUT));
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
                api->editMessage(sent, res->get(Strings::LLM_NO_MESSAGE_OUTPUT));
                return;
            }
            {
                std::lock_guard<std::mutex> lock(response_keys_mutex);
                if (chatResponse.response_id) {
                    response_keys[chat_id] = *chatResponse.response_id;
                } else {
                    response_keys.erase(chat_id);
                }
            }
            std::initializer_list<
                std::pair<absl::string_view, absl::string_view>>
                replacements = {
                    {"`", ""},    {"_", "\\_"}, {"*", "\\*"}, {"[", "\\["},
                    {"]", "\\]"}, {"(", "\\("}, {")", "\\)"}, {"~", "\\~"},
                    {"`", "\\`"}, {">", "\\>"}, {"#", "\\#"}, {"+", "\\+"},
                    {"-", "\\-"}, {"=", "\\="}, {"|", "\\|"}, {"{", "\\{"},
                    {"}", "\\}"}, {".", "\\."}, {"!", "\\!"}};
            ;
            auto replaced = absl::StrReplaceAll(it->content, replacements);
            api->editMessage<TgBotApi::ParseMode::MarkdownV2>(
                sent, std::move(replaced));
        } catch (const std::exception& e) {
            LOG(ERROR) << "Failed to parse LLM server response: " << e.what();
            api->editMessage(sent,
                             res->get(Strings::LLM_PARSE_RESPONSE_FAILED));
        }
    } else {
        LOG(ERROR) << "Failed to get response from LLM server.";
        api->editMessage(sent, res->get(Strings::LLM_RESPONSE_FAILED));
    }
}

DECLARE_COMMAND_HANDLER(ask) {
    auto mgr = provider->config.get();

    if (!mgr->get(ConfigManager::Configs::LLM_TYPE) ||
        !mgr->get(ConfigManager::Configs::LLM_LOCATION)) {
        api->sendMessage(
            message->get<MessageAttrs::Chat>(),
            res->get(Strings::LLM_NOT_CONFIGURED));
        return;
    }

    std::string type = *mgr->get(ConfigManager::Configs::LLM_TYPE);
    std::string location = *mgr->get(ConfigManager::Configs::LLM_LOCATION);
    auto authkey = mgr->get(ConfigManager::Configs::LLM_AUTHKEY);

    LOG(INFO) << "LLM Configuration - Type: " << type
              << ", Path/URL: " << location;

    if (type == "local") {
#ifdef ASK_ENABLE_LOCAL_LLM
        localmodelhandler(api, message, res, location);
#else
        LOG(ERROR) << "Local LLM support is not enabled in this build.";
        api->sendMessage(
            message->get<MessageAttrs::Chat>(),
            res->get(Strings::LLM_LOCAL_NOT_ENABLED));
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
        api->sendMessage(message->get<MessageAttrs::Chat>(),
                         res->get(Strings::LLM_UNSUPPORTED_TYPE));
    }
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "ask",
    .description = "Ask a query to an LLM",
    .function = COMMAND_HANDLER_NAME(ask),
};
