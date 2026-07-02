#include <absl/log/log.h>
#include <absl/strings/ascii.h>
#include <absl/strings/str_replace.h>
#include <fmt/format.h>
#include <tgbot/TgException.h>

#include <algorithm>
#include <api/AuthContext.hpp>
#include <api/CommandModule.hpp>
#include <api/MarkdownV2.hpp>
#include <api/MessageExt.hpp>
#include <api/Providers.hpp>
#include <api/StringResLoader.hpp>
#include <api/TgBotApi.hpp>
#include <database/DatabaseBase.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "AskConfirmTool.hpp"
#include "ModelPickerTool.hpp"
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

using llm::model_picker::selectedModel;
using llm::model_picker::setSelectedModel;

// Admin-only tool: lets the model DM an arbitrary Telegram user on its own
// initiative. Only ever offered to the LLM when the invoking user passes the
// AdminUser auth check in the handler below.
const llm::Tool kSendMessageTool{
    "send_message",
    "Send a direct message to a Telegram user by their numeric user ID. "
    "Only use this when explicitly asked to message a user.",
    nlohmann::json{
        {"type", "object"},
        {"properties",
         {{"user_id", {{"type", "integer"}}}, {"text", {{"type", "string"}}}}},
        {"required", {"user_id", "text"}}}};

llm::ToolExecutor makeSendMessageExecutor(TgBotApi::Ptr api) {
    return [api](const std::string& /*name*/, const nlohmann::json& input,
                bool& isError) -> std::string {
        isError = false;
        try {
            const auto userId = input.at("user_id").get<std::int64_t>();
            const auto text = input.at("text").get<std::string>();
            api->sendMessage(userId, text);
            return fmt::format("Message sent successfully to user {}.", userId);
        } catch (const TgBot::TgException& ex) {
            isError = true;
            return fmt::format("Failed to send message: {}", ex.what());
        } catch (const std::exception& ex) {
            isError = true;
            return fmt::format("Invalid tool input: {}", ex.what());
        }
    };
}

// Admin-only tools backed by DatabaseBase's chatmap (name<->id registry).
// Only entries an admin has manually registered (via save_chat_info below,
// /setChatAlias, or the DatabaseCtrl CLI) are resolvable here - there is no
// passive auto-population from message traffic, so a "not found" result
// doesn't mean the id/name is invalid, just unregistered. Descriptions
// cross-reference each other so the model picks get vs. save based on
// whether the admin is asking to look something up or to remember it.
const llm::Tool kGetChatIdTool{
    "get_chat_id",
    "Retrieve the numeric Telegram chat/user ID for a name that was "
    "previously saved with save_chat_info. Use this when asked to find, "
    "look up, retrieve, or check an ID - never to create or change a "
    "registration. Returns a not-found message if the name is unknown.",
    nlohmann::json{{"type", "object"},
                  {"properties", {{"name", {{"type", "string"}}}}},
                  {"required", {"name"}}}};

llm::ToolExecutor makeGetChatIdExecutor(const Providers* provider) {
    return [provider](const std::string& /*name*/, const nlohmann::json& input,
                      bool& isError) -> std::string {
        isError = false;
        try {
            const auto name = input.at("name").get<std::string>();
            const auto id = provider->database->getChatId(name);
            if (!id) {
                return fmt::format("No chat/user id found for name \"{}\".",
                                  name);
            }
            return fmt::format("{}", *id);
        } catch (const std::exception& ex) {
            isError = true;
            return fmt::format("Invalid tool input: {}", ex.what());
        }
    };
}

const llm::Tool kGetChatNameTool{
    "get_chat_name",
    "Retrieve the name previously saved (via save_chat_info) for a numeric "
    "Telegram chat/user ID. Use this when asked to find, look up, retrieve, "
    "or check a name - never to create or change a registration. Returns a "
    "not-found message if the ID is unregistered.",
    nlohmann::json{{"type", "object"},
                  {"properties", {{"chat_id", {{"type", "integer"}}}}},
                  {"required", {"chat_id"}}}};

llm::ToolExecutor makeGetChatNameExecutor(const Providers* provider) {
    return [provider](const std::string& /*name*/, const nlohmann::json& input,
                      bool& isError) -> std::string {
        isError = false;
        try {
            const auto chatId = input.at("chat_id").get<ChatId>();
            const auto name = provider->database->getChatName(chatId);
            if (!name) {
                return fmt::format("No name registered for chat/user id {}.",
                                  chatId);
            }
            return *name;
        } catch (const std::exception& ex) {
            isError = true;
            return fmt::format("Invalid tool input: {}", ex.what());
        }
    };
}

const llm::Tool kSaveChatInfoTool{
    "save_chat_info",
    "Register a new mapping between a numeric Telegram chat/user ID and a "
    "name, so it can be found later with get_chat_id/get_chat_name. Use "
    "this whenever explicitly asked to save, register, remember, add, or "
    "create an association between an ID and a name - never for lookups. "
    "Fails if that ID is already registered (check with get_chat_name "
    "first if unsure).",
    nlohmann::json{
        {"type", "object"},
        {"properties",
         {{"chat_id", {{"type", "integer"}}}, {"name", {{"type", "string"}}}}},
        {"required", {"chat_id", "name"}}}};

llm::ToolExecutor makeSaveChatInfoExecutor(const Providers* provider) {
    return [provider](const std::string& /*name*/, const nlohmann::json& input,
                      bool& isError) -> std::string {
        isError = false;
        try {
            const auto chatId = input.at("chat_id").get<ChatId>();
            const auto chatName = input.at("name").get<std::string>();
            switch (provider->database->addChatInfo(chatId, chatName)) {
                case DatabaseBase::AddResult::OK:
                    return fmt::format("Saved: {} -> \"{}\".", chatId,
                                      chatName);
                case DatabaseBase::AddResult::ALREADY_EXISTS:
                    isError = true;
                    return fmt::format(
                        "Chat/user id {} is already registered (use "
                        "get_chat_name to see the existing name).",
                        chatId);
                case DatabaseBase::AddResult::BACKEND_ERROR:
                    isError = true;
                    return "Failed to save due to a database error.";
            }
            isError = true;
            return "Unknown save result.";
        } catch (const std::exception& ex) {
            isError = true;
            return fmt::format("Invalid tool input: {}", ex.what());
        }
    };
}

// Dispatches by tool name to whichever admin tool was actually called; each
// underlying executor already ignores the `name` parameter it's handed.
llm::ToolExecutor makeCombinedExecutor(TgBotApi::Ptr api, ChatId chatId,
                                       const Providers* provider) {
    auto sendMsg = makeSendMessageExecutor(api);
    auto askConfirm = llm::ask_confirm::makeAskConfirmExecutor(api, chatId);
    auto getChatId = makeGetChatIdExecutor(provider);
    auto getChatName = makeGetChatNameExecutor(provider);
    auto saveChatInfo = makeSaveChatInfoExecutor(provider);
    return [sendMsg, askConfirm, getChatId, getChatName, saveChatInfo](
               const std::string& name, const nlohmann::json& input,
               bool& isError) -> std::string {
        if (name == "send_message") {
            return sendMsg(name, input, isError);
        }
        if (name == "ask") {
            return askConfirm(name, input, isError);
        }
        if (name == "get_chat_id") {
            return getChatId(name, input, isError);
        }
        if (name == "get_chat_name") {
            return getChatName(name, input, isError);
        }
        if (name == "save_chat_info") {
            return saveChatInfo(name, input, isError);
        }
        isError = true;
        return fmt::format("Unknown tool: {}", name);
    };
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
        std::vector<std::string> modelIds;
        modelIds.reserve(models.size());
        for (const auto& model : models) {
            list += fmt::format("{}. {}", index++, model.id);
            if (model.display != model.id && !model.display.empty()) {
                list += fmt::format(" ({})", model.display);
            }
            list += '\n';
            modelIds.push_back(model.id);
        }
        auto keyboard =
            llm::model_picker::startPicker(api, chatId, std::move(modelIds));
        api->sendReplyMessage(
            message->message(),
            fmt::format(fmt::runtime(res->get(Strings::LLM_MODELS_AVAILABLE)),
                        list),
            keyboard);
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

    api->sendReplyMessage(message->message(),
                          res->get(Strings::LLM_PROCESSING_QUERY));
    const bool isAdmin = provider->auth->isAuthorized(
        message->message(), AuthContext::AccessLevel::AdminUser);
    const auto answer =
        isAdmin ? backend->chat(model, SYSTEM_PROMPT, query, chatId,
                                {kSendMessageTool, llm::ask_confirm::kAskConfirmTool,
                                 kGetChatIdTool, kGetChatNameTool,
                                 kSaveChatInfoTool},
                                makeCombinedExecutor(api, chatId, provider))
                : backend->chat(model, SYSTEM_PROMPT, query, chatId);
    if (!answer) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::LLM_RESPONSE_FAILED));
        return;
    }
    try {
        api->sendReplyMessage<TgBotApi::ParseMode::MarkdownV2>(
            message->message(), tgbot::markdownv2::escape(*answer));
    } catch (const TgBot::TgException& ex) {
        LOG(WARNING) << "MarkdownV2 send failed, sending plain: " << ex.what();
        api->sendReplyMessage(message->message(), *answer);
    }
}

}  // namespace

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "ask",
    .description = "Ask a query to an LLM",
    .function = COMMAND_HANDLER_NAME(ask),
};
