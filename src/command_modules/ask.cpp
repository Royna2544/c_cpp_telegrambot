#include <absl/log/log.h>
#include <absl/strings/ascii.h>
#include <absl/strings/str_replace.h>
#include <fmt/format.h>
#include <tgbot/TgException.h>

#include <algorithm>
#include <api/CommandModule.hpp>
#include <api/MarkdownV2.hpp>
#include <api/MessageExt.hpp>
#include <api/Providers.hpp>
#include <api/StringResLoader.hpp>
#include <api/TgBotApi.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "llm/AnthropicApi.hpp"
#include "llm/LLMBackend.hpp"
#include "llm/LMStudioBackend.hpp"
#include "llm/OpenAIApi.hpp"
#include "llm/SYSTEM_PROMPT.hpp"
#include "utils/ConfigManager.hpp"

namespace llm {
std::unique_ptr<LLMBackend> makeBackend(LLMApiType type, std::string url,
                                        std::string authkey) {
    switch (type) {
        case LLMApiType::OpenAI:
            return std::make_unique<openai::OpenAIBackend>(std::move(url),
                                                           std::move(authkey));
        case LLMApiType::LMStudio:
            return std::make_unique<LMStudioBackend>(std::move(url),
                                                     std::move(authkey));
        case LLMApiType::Anthropic:
            return std::make_unique<anthropic::AnthropicBackend>(
                std::move(url), std::move(authkey));
    }
    return nullptr;
}
}  // namespace llm

namespace {

// Per-chat selected model (in-memory; resets on restart). Both accessors share
// one store so a selection made by setSelectedModel is visible to readers.
std::pair<std::mutex&, std::unordered_map<ChatId, std::string>&>
selectedModelStore() {
    static std::mutex mtx;
    static std::unordered_map<ChatId, std::string> map;
    return {mtx, map};
}
std::string selectedModel(ChatId chatId) {
    auto [mtx, map] = selectedModelStore();
    const std::lock_guard lock(mtx);
    auto it = map.find(chatId);
    return it == map.end() ? std::string{} : it->second;
}
void setSelectedModel(ChatId chatId, std::string model) {
    auto [mtx, map] = selectedModelStore();
    const std::lock_guard lock(mtx);
    map[chatId] = std::move(model);
}

DECLARE_COMMAND_HANDLER(ask) {
    auto* mgr = provider->config.get();
    const auto urlOpt = mgr->get(ConfigManager::Configs::LLM_URL);
    const auto typeOpt = mgr->get(ConfigManager::Configs::LLM_API_TYPE);
    if (!urlOpt || !typeOpt) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::LLM_NOT_CONFIGURED));
        return;
    }
    const auto apiType = llm::parseApiType(*typeOpt);
    if (!apiType) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::LLM_UNSUPPORTED_TYPE));
        return;
    }
    const std::string authkey =
        mgr->get(ConfigManager::Configs::LLM_AUTHKEY).value_or("");
    auto backend = llm::makeBackend(*apiType, *urlOpt, authkey);

    const ChatId chatId = message->get<MessageAttrs::Chat>()->id;

    const std::string text =
        message->has<MessageAttrs::ExtraText>()
            ? message->get<MessageAttrs::ExtraText>()
            : std::string{};
    const std::string_view trimmed = absl::StripAsciiWhitespace(text);
    if (trimmed.empty()) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::LLM_PROVIDE_QUERY));
        return;
    }

    // Split the first whitespace-delimited token (subcommand) from the rest.
    const auto sep = trimmed.find_first_of(" \t\n");
    const std::string_view first =
        sep == std::string_view::npos ? trimmed : trimmed.substr(0, sep);
    const std::string_view rest =
        sep == std::string_view::npos
            ? std::string_view{}
            : absl::StripAsciiWhitespace(trimmed.substr(sep + 1));

    if (first == "models") {
        const auto models = backend->listModels();
        if (models.empty()) {
            api->sendReplyMessage(message->message(),
                                  res->get(Strings::LLM_NO_MODELS));
            return;
        }
        std::string list;
        int index = 1;
        for (const auto& model : models) {
            list += fmt::format("{}. {}", index++, model.id);
            if (model.display != model.id && !model.display.empty()) {
                list += fmt::format(" ({})", model.display);
            }
            list += '\n';
        }
        api->sendReplyMessage(
            message->message(),
            fmt::format(fmt::runtime(res->get(Strings::LLM_MODELS_AVAILABLE)),
                        list));
        return;
    }

    if (first == "model") {
        if (rest.empty()) {
            api->sendReplyMessage(message->message(),
                                  res->get(Strings::LLM_PROVIDE_QUERY));
            return;
        }
        const std::string wanted(rest);
        const auto models = backend->listModels();
        const bool found = std::ranges::any_of(
            models, [&](const llm::LLMModel& m) { return m.id == wanted; });
        if (!found) {
            api->sendReplyMessage(
                message->message(),
                fmt::format(
                    fmt::runtime(res->get(Strings::LLM_MODEL_NOT_FOUND)),
                    wanted));
            return;
        }
        setSelectedModel(chatId, wanted);
        api->sendReplyMessage(
            message->message(),
            fmt::format(fmt::runtime(res->get(Strings::LLM_MODEL_SET)), wanted));
        return;
    }

    // Otherwise the whole text is the query.
    const std::string query(trimmed);
    std::string model = selectedModel(chatId);
    if (model.empty()) {
        const auto models = backend->listModels();
        if (models.empty()) {
            api->sendReplyMessage(message->message(),
                                  res->get(Strings::LLM_NO_MODELS));
            return;
        }
        model = models.front().id;
    }

    auto sent = api->sendReplyMessage(message->message(),
                                      res->get(Strings::LLM_PROCESSING_QUERY));
    const auto answer = backend->chat(model, SYSTEM_PROMPT, query, chatId);
    if (!answer) {
        api->editMessage(sent, res->get(Strings::LLM_RESPONSE_FAILED));
        return;
    }
    try {
        api->editMessage<TgBotApi::ParseMode::MarkdownV2>(
            sent, tgbot::markdownv2::escape(*answer));
    } catch (const TgBot::TgException& ex) {
        LOG(WARNING) << "MarkdownV2 edit failed, sending plain: " << ex.what();
        api->editMessage(sent, *answer);
    }
}

}  // namespace

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "ask",
    .description = "Ask a query to an LLM",
    .function = COMMAND_HANDLER_NAME(ask),
};
