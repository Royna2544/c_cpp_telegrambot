#include <AbslLogCompat.hpp>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <tgbot/TgException.h>

#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <api/TgBotApi.hpp>
#include <mutex>

#include "llm/LLMCore.hpp"

DECLARE_COMMAND_HANDLER(ask) {
    LLMCore llm;
    LLMCore::Model model;

    auto sent = api->sendReplyMessage(message->message(),
                                      "Processing your query, please wait...");
    api->editMessage(sent, "Initializing LLM core, please wait...");
    if (!model.load("/mnt/c/Users/royna/Documents/"
                    "OpenAI-20B-NEOPlus-Uncensored-IQ4_NL.gguf")) {
        LOG(ERROR) << "Failed to initialize LLM core.";
        api->editMessage(sent, "Error: Unable to initialize LLM core.");
        return;
    }

    std::string query = message->get<MessageAttrs::ExtraText>();
    auto response_opt = llm.query(&model, query, 512);
    if (!response_opt) {
        LOG(ERROR) << "LLM query failed.";
        api->editMessage(sent, "Error: LLM query failed.");
        return;
    }
    const auto& response = *response_opt;
    std::string reply =
        fmt::format(R"(
Thought: {}
)",
                    response.thought);
    api->editMessage(sent, reply);
    api->sendMessage(message->get<MessageAttrs::Chat>(), 
                     fmt::format("Answer: {}", response.answer));   
    api->sendMessage(message->get<MessageAttrs::Chat>(), fmt::format(
        "Query processed in {:%S} seconds.",response.duration));
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "ask",
    .description = "Ask a query to an LLM",
    .function = COMMAND_HANDLER_NAME(ask),
};
