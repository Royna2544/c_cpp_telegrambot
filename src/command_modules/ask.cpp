#include <absl/log/log.h>
#include <fmt/format.h>
#include <fmt/chrono.h>
#include <tgbot/TgException.h>

#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <api/TgBotApi.hpp>

#include "llm/LLMCore.hpp"

DECLARE_COMMAND_HANDLER(ask) {
    LLMCore llm;
    LLMCore::Model model;

    auto sent = api->sendMessage(
        message->get<MessageAttrs::Chat>(),
                     "Processing your query, please wait...");
    // Load a model (path hardcoded for example purposes)
    if (!model.load("C:\\Users\\royna\\Documents\\mistral-small-24b-instruct-2501-reasoning.Q3_K_M.gguf")) {
        LOG(ERROR) << "Failed to load LLM model.";
        api->editMessage(sent,
                              "Error: Unable to load the language model.");
        return;
    }

    std::string query = message->get<MessageAttrs::ExtraText>();
    auto response_opt = llm.query(&model, query);
    if (!response_opt) {
        LOG(ERROR) << "LLM query failed.";
        api->editMessage(sent, "Error: LLM query failed.");
        return;
    }
    const auto& response = *response_opt;
    std::string reply =
        fmt::format(R"(
Answer: {}
Duration: {:%S seconds}
)", response.answer, response.duration);
    api->editMessage(sent, reply);
    model.cleanup();

}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "ask",
    .description = "Ask a query to an LLM",
    .function = COMMAND_HANDLER_NAME(ask),
    .valid_args =
        {
            .enabled = true,
            .counts = DynModule::craftArgCountMask<1>(),
            .split_type = DynModule::ValidArgs::Split::None,
            .usage = "/ask [Query]",
        },
};
