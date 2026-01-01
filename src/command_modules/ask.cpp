#include <absl/log/log.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <tgbot/TgException.h>

#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <api/TgBotApi.hpp>
#include <mutex>

#include "llm/LLMCore.hpp"

constexpr const char* kLLMStorageKey = "LLMCoreInstance";

struct LLMCoreInstance {
    LLMCore llm;
    LLMCore::Model model;
};

DECLARE_COMMAND_HANDLER(ask) {
    LLMCoreInstance* instance;

    auto sent = api->sendReplyMessage(message->message(),
                                      "Processing your query, please wait...");

    if (provider->globalStorage->hasStorage(kLLMStorageKey)) {
        instance = provider->globalStorage->getStorage(kLLMStorageKey)
                       .getAs<LLMCoreInstance>();
    } else {
        provider->globalStorage->getStorage(kLLMStorageKey) =
            MakeSharedMallocFrom<LLMCoreInstance>();

        instance = provider->globalStorage->getStorage(kLLMStorageKey)
                       .getAs<LLMCoreInstance>();

        api->editMessage(sent, "Initializing LLM core, please wait...");
        if (!instance->model.load("/mnt/c/Users/royna/Documents/"
                                  "gpt-oss-20b-Q5_K_M.gguf")) {
            LOG(ERROR) << "Failed to initialize LLM core.";
            api->editMessage(sent, "Error: Unable to initialize LLM core.");
            provider->globalStorage->removeStorage(kLLMStorageKey);
            return;
        }
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

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "ask",
    .description = "Ask a query to an LLM",
    .function = COMMAND_HANDLER_NAME(ask),
};
