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
#include <chrono>
#include <database/DatabaseBase.hpp>
#include <memory>
#include <mutex>
#include <stop_token>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "AskConfirmTool.hpp"
#include "BuilderLaunchTool.hpp"
#include "ModelPickerTool.hpp"
#include "ToolRouter.hpp"
#include "llm/AnthropicApi.hpp"
#include "llm/LLMBackend.hpp"
#include "llm/LMStudioBackend.hpp"
#include "llm/OpenAIApi.hpp"
#include "llm/OutboundDispatch.hpp"
#include "llm/SYSTEM_PROMPT.hpp"
#include "llm/TelegramOutput.hpp"
#include "llm/ToolSafety.hpp"
#include "utils/ConfigManager.hpp"

namespace llm {
std::unique_ptr<LLMBackend> makeBackend(LLMApiType type, std::string url,
                                        std::string authkey,
                                        std::function<bool()> cancelled) {
    switch (type) {
        case LLMApiType::OpenAI:
            return std::make_unique<openai::OpenAIBackend>(
                std::move(url), std::move(authkey), cancelled);
        case LLMApiType::LMStudio:
            return std::make_unique<LMStudioBackend>(
                std::move(url), std::move(authkey), cancelled);
        case LLMApiType::Anthropic:
            return std::make_unique<anthropic::AnthropicBackend>(
                std::move(url), std::move(authkey), cancelled);
    }
    return nullptr;
}
}  // namespace llm

namespace {

using llm::model_picker::selectedModel;
using llm::model_picker::setSelectedModel;

constexpr auto kAskWorkDeadline = std::chrono::minutes(7);

bool queuePlainReply(TgBotApi::Ptr api, Message::Ptr sourceMessage,
                     std::string_view text,
                     TgBot::GenericReply::Ptr replyMarkup = nullptr) {
    std::string ownedText(text);
    const bool queued = llm::outbound::post(
        api, [api, sourceMessage = std::move(sourceMessage),
              text = std::move(ownedText),
              replyMarkup = std::move(replyMarkup)](std::stop_token stop) {
            if (!stop.stop_requested()) {
                api->sendReplyMessage(sourceMessage, text, replyMarkup);
            }
        });
    if (!queued) {
        LOG(ERROR) << "Outbound lane rejected an /ask reply";
    }
    return queued;
}

bool queueMarkdownReply(TgBotApi::Ptr api, Message::Ptr sourceMessage,
                        std::string text) {
    const bool queued = llm::outbound::post(
        api, [api, sourceMessage = std::move(sourceMessage),
              text = std::move(text)](std::stop_token stop) {
            if (stop.stop_requested()) {
                return;
            }
            try {
                api->sendReplyMessage<TgBotApi::ParseMode::MarkdownV2>(
                    sourceMessage, tgbot::markdownv2::escape(text));
            } catch (const TgBot::TgException& ex) {
                LOG(WARNING)
                    << "MarkdownV2 send failed, sending plain: " << ex.what();
                try {
                    api->sendReplyMessage(sourceMessage, text);
                } catch (const TgBot::TgException& plainEx) {
                    LOG(ERROR) << "Plain LLM reply chunk also failed: "
                               << plainEx.what();
                }
            }
        });
    if (!queued) {
        LOG(ERROR) << "Outbound lane rejected an /ask response chunk";
    }
    return queued;
}

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

llm::ToolExecutor makeSendMessageExecutor(TgBotApi::Ptr api,
                                          std::stop_token cancellation) {
    return [api, cancellation](const std::string& /*name*/,
                               const nlohmann::json& input,
                               bool& isError) -> std::string {
        isError = false;
        try {
            const auto userId = input.at("user_id").get<std::int64_t>();
            const auto text = input.at("text").get<std::string>();
            const auto sent = llm::outbound::invoke(
                api,
                [api, userId, text] { return api->sendMessage(userId, text); },
                cancellation);
            if (!sent || !*sent) {
                isError = true;
                return "Failed to send message: outbound queue unavailable or "
                       "timed out.";
            }
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

std::vector<llm::Tool> toolsForDomain(llm::tool_router::Domain domain) {
    std::vector<llm::Tool> tools;
    for (const auto name : llm::tool_router::toolNamesForDomain(domain)) {
        if (name == kSendMessageTool.name) {
            tools.push_back(kSendMessageTool);
        } else if (name == llm::ask_confirm::kAskConfirmTool.name) {
            tools.push_back(llm::ask_confirm::kAskConfirmTool);
        } else if (name == kGetChatIdTool.name) {
            tools.push_back(kGetChatIdTool);
        } else if (name == kGetChatNameTool.name) {
            tools.push_back(kGetChatNameTool);
        } else if (name == kSaveChatInfoTool.name) {
            tools.push_back(kSaveChatInfoTool);
        } else if (name == llm::builder_launch::kKernelBuildTool.name) {
            tools.push_back(llm::builder_launch::kKernelBuildTool);
        } else if (name == llm::builder_launch::kRomBuildTool.name) {
            tools.push_back(llm::builder_launch::kRomBuildTool);
        }
    }
    return tools;
}

// True for every route that actually exposes kernelbuild/rombuild, so the
// build orchestration addendum and pending-plan context accompany the tools
// the model can see (Domain::All included - it exposes them too).
bool isBuilderDomain(llm::tool_router::Domain domain) {
    using llm::tool_router::Domain;
    return domain == Domain::KernelBuild || domain == Domain::RomBuild ||
           domain == Domain::Build || domain == Domain::All;
}

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
llm::ToolExecutor makeCombinedExecutor(
    TgBotApi::Ptr api, ChatId chatId, UserId initiatingUserId,
    const Providers* provider, Message::Ptr sourceMessage,
    const std::vector<llm::Tool>& allowedTools, std::stop_token cancellation) {
    auto policy =
        std::make_shared<llm::tool_safety::ToolExecutionPolicy>(allowedTools);
    auto sendMsg = makeSendMessageExecutor(api, cancellation);
    auto askConfirm = llm::ask_confirm::makeAskConfirmExecutor(
        api, chatId, initiatingUserId, provider->auth.get(),
        [policy](const std::string& toolName, const nlohmann::json& input) {
            return policy->recordApproval(toolName, input);
        },
        cancellation);
    auto getChatId = makeGetChatIdExecutor(provider);
    auto getChatName = makeGetChatNameExecutor(provider);
    auto saveChatInfo = makeSaveChatInfoExecutor(provider);
    auto launchBuilder =
        llm::builder_launch::makeExecutor(api, std::move(sourceMessage));
    llm::ToolExecutor dispatch =
        [sendMsg, askConfirm, getChatId, getChatName, saveChatInfo,
         launchBuilder](const std::string& name, const nlohmann::json& input,
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
        if (name == llm::builder_launch::kKernelBuildTool.name ||
            name == llm::builder_launch::kRomBuildTool.name) {
            return launchBuilder(name, input, isError);
        }
        isError = true;
        return fmt::format("Unknown tool: {}", name);
    };
    return [policy, dispatch = std::move(dispatch)](
               const std::string& name, const nlohmann::json& input,
               bool& isError) -> std::string {
        return policy->execute(name, input, dispatch, isError);
    };
}

void runAskWork(TgBotApi::Ptr api, Message::Ptr sourceMessage, std::string text,
                const StringResLoader::PerLocaleMap* res,
                const Providers* provider, std::stop_token stop) {
    if (stop.stop_requested()) {
        return;
    }
    auto* mgr = provider->config.get();
    const auto urlOpt = mgr->get(ConfigManager::Configs::LLM_URL);
    const auto typeOpt = mgr->get(ConfigManager::Configs::LLM_API_TYPE);
    if (!urlOpt || !typeOpt) {
        queuePlainReply(api, sourceMessage,
                        res->get(Strings::LLM_NOT_CONFIGURED));
        return;
    }
    const auto apiType = llm::parseApiType(*typeOpt);
    if (!apiType) {
        queuePlainReply(api, sourceMessage,
                        res->get(Strings::LLM_UNSUPPORTED_TYPE));
        return;
    }
    const std::string authkey =
        mgr->get(ConfigManager::Configs::LLM_AUTHKEY).value_or("");
    auto backend = llm::makeBackend(*apiType, *urlOpt, authkey,
                                    [stop] { return stop.stop_requested(); });

    const ChatId chatId = sourceMessage->chat->id;
    const std::string_view trimmed = absl::StripAsciiWhitespace(text);
    if (trimmed.empty()) {
        queuePlainReply(api, sourceMessage,
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
        if (stop.stop_requested()) {
            return;
        }
        if (models.empty()) {
            queuePlainReply(api, sourceMessage,
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
        auto keyboard = llm::model_picker::startPicker(
            api, chatId, sourceMessage->from.value()->id, std::move(modelIds));
        const auto rendered = fmt::format(
            fmt::runtime(res->get(Strings::LLM_MODELS_AVAILABLE)), list);
        const auto chunks = llm::telegram_output::splitPlain(rendered);
        for (std::size_t i = 0; i < chunks.size(); ++i) {
            if (stop.stop_requested() ||
                !queuePlainReply(api, sourceMessage, chunks[i],
                                 i == 0 ? keyboard : nullptr)) {
                break;
            }
        }
        return;
    }

    if (first == "model") {
        if (rest.empty()) {
            queuePlainReply(api, sourceMessage,
                            res->get(Strings::LLM_PROVIDE_QUERY));
            return;
        }
        const std::string wanted(rest);
        const auto models = backend->listModels();
        if (stop.stop_requested()) {
            return;
        }
        const bool found = std::ranges::any_of(
            models, [&](const llm::LLMModel& m) { return m.id == wanted; });
        if (!found) {
            queuePlainReply(
                api, sourceMessage,
                fmt::format(
                    fmt::runtime(res->get(Strings::LLM_MODEL_NOT_FOUND)),
                    wanted));
            return;
        }
        setSelectedModel(chatId, wanted);
        queuePlainReply(
            api, sourceMessage,
            fmt::format(fmt::runtime(res->get(Strings::LLM_MODEL_SET)),
                        wanted));
        return;
    }

    // Otherwise the whole text is the query.
    const std::string query(trimmed);
    std::string model = selectedModel(chatId);
    if (model.empty()) {
        const auto models = backend->listModels();
        if (stop.stop_requested()) {
            return;
        }
        if (models.empty()) {
            queuePlainReply(api, sourceMessage,
                            res->get(Strings::LLM_NO_MODELS));
            return;
        }
        model = models.front().id;
    }

    queuePlainReply(api, sourceMessage,
                    res->get(Strings::LLM_PROCESSING_QUERY));
    if (stop.stop_requested()) {
        queuePlainReply(api, sourceMessage,
                        res->get(Strings::LLM_RESPONSE_FAILED));
        return;
    }
    const bool isAdmin = provider->auth->isAuthorized(
        sourceMessage, AuthContext::AccessLevel::AdminUser,
        AuthContext::MessageAgePolicy::AuthenticatedInternal);
    const auto userId = sourceMessage->from.value()->id;
    auto toolDomain = llm::tool_router::Domain::Chat;
    std::vector<llm::Tool> tools;
    if (isAdmin) {
        const auto pending = llm::builder_launch::pendingBuilds(chatId, userId);
        const llm::tool_router::PendingBuilds pendingRoute{
            .kernel = pending.kernel,
            .rom = pending.rom,
        };
        toolDomain = llm::tool_router::selectDomain(
            query, pendingRoute,
            [&](std::string_view systemPrompt,
                std::string_view userInput) -> std::optional<std::string> {
                return backend->classify(model, std::string(systemPrompt),
                                         std::string(userInput));
            });
        if (stop.stop_requested()) {
            queuePlainReply(api, sourceMessage,
                            res->get(Strings::LLM_RESPONSE_FAILED));
            return;
        }
        tools = toolsForDomain(toolDomain);
        if (toolDomain == llm::tool_router::Domain::All) {
            LOG(WARNING) << "LLM capability router could not classify the "
                            "request; exposing all "
                         << tools.size() << " tools as a fallback";
        } else {
            LOG(INFO) << "LLM tool route: "
                      << llm::tool_router::name(toolDomain) << " ("
                      << tools.size() << " exposed tools)";
        }
    }

    std::string systemPrompt = SYSTEM_PROMPT;
    if (isAdmin && isBuilderDomain(toolDomain)) {
        systemPrompt += R"(

### Admin build orchestration
- When the admin expresses any intent to build, prepare, or compile a Linux
  kernel or Android ROM/recovery, you MUST call kernelbuild or rombuild before
  replying. This applies even when the request is incomplete.
- Never ask for a missing build field from your own reasoning. First call the
  builder tool with every field the admin supplied; its result tells you which
  fields to ask for.
- A short reply to a pending build question (for example "a30") is the value
  for that plan's missing field. Call the same builder tool to update the plan;
  do not ask what the admin wants to do with that value.
- Do not invent build fields. Omit fields the admin did not specify so server
  defaults and compatibility validation remain authoritative.
)";
        systemPrompt += llm::builder_launch::pendingContext(chatId, userId);
    }
    const auto answer =
        isAdmin && !tools.empty()
            ? backend->chat(model, systemPrompt, query, chatId, tools,
                            makeCombinedExecutor(api, chatId, userId, provider,
                                                 sourceMessage, tools, stop))
            : backend->chat(model, systemPrompt, query, chatId);
    if (stop.stop_requested()) {
        queuePlainReply(api, sourceMessage,
                        res->get(Strings::LLM_RESPONSE_FAILED));
        return;
    }
    if (!answer || answer->empty()) {
        queuePlainReply(api, sourceMessage,
                        res->get(Strings::LLM_RESPONSE_FAILED));
        return;
    }
    const auto chunks = llm::telegram_output::splitForMarkdown(*answer);
    for (const auto& chunk : chunks) {
        if (stop.stop_requested()) {
            queuePlainReply(api, sourceMessage,
                            res->get(Strings::LLM_RESPONSE_FAILED));
            break;
        }
        if (!queueMarkdownReply(api, sourceMessage, chunk)) {
            break;
        }
    }
}

DECLARE_COMMAND_HANDLER(ask) {
    const auto sourceMessage = message->message();
    std::string queryText = message->has<MessageAttrs::ExtraText>()
                                ? message->get<MessageAttrs::ExtraText>()
                                : std::string{};
    const auto workId = api->submitCommandWork(
        "ask", TgBotApi::WorkClass::Llm,
        [api, sourceMessage, queryText = std::move(queryText), res,
         provider](std::stop_token stop) mutable {
            try {
                runAskWork(api, sourceMessage, std::move(queryText), res,
                           provider, stop);
            } catch (const std::exception& ex) {
                LOG(ERROR) << "/ask LLM work failed: " << ex.what();
                queuePlainReply(api, sourceMessage,
                                res->get(Strings::LLM_RESPONSE_FAILED));
            } catch (...) {
                LOG(ERROR) << "/ask LLM work failed with an unknown exception";
                queuePlainReply(api, sourceMessage,
                                res->get(Strings::LLM_RESPONSE_FAILED));
            }
        },
        {.deadline = kAskWorkDeadline});
    if (!workId) {
        LOG(WARNING) << "LLM lane rejected /ask work";
        queuePlainReply(api, sourceMessage,
                        res->get(Strings::LLM_RESPONSE_FAILED));
    }
}

}  // namespace

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "ask",
    .description = "Ask a query to an LLM",
    .function = COMMAND_HANDLER_NAME(ask),
};
