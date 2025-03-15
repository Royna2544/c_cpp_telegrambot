#include <LogSinks.hpp>
#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>

DECLARE_COMMAND_HANDLER(log) {
    auto chat = message->get<MessageAttrs::Chat>();
    try {
        api->sendDocument(
            chat, TgBot::InputFile::fromFile(
                      std::filesystem::temp_directory_path() / kDefaultLogFile,
                      "text/plain"));
    } catch (const std::ifstream::failure& ex) {
        api->sendMessage(chat, ex.what());
    }
}

extern "C" const struct DynModule DYN_COMMAND_EXPORT DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::HideDescription | DynModule::Flags::Enforced,
    .name = "log",
    .description = "Get logs",
    .function = COMMAND_HANDLER_NAME(log),
};
