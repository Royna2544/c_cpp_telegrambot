#include "CommandParser.hpp"

#include <TryParseStr.hpp>
#include <absl/log/log.h>
#include <bot/FileHelperNew.hpp>

namespace TgBotSocket::Client {

template <class C>
bool CommandParser::parseEnum(ByteHelper<C>* res, C max, const char* str, std::string_view name) {
    int parsed{};
    if (try_parse(str, &parsed)) {
        if (parsed >= 0 && parsed < static_cast<int>(max)) {
            *res = static_cast<C>(parsed);
            return true;
        }
        LOG(ERROR) << "Cannot convert " << str << " to " << name << " enum value";
    }
    return false;
}

std::optional<Command> CommandParser::parseCommand(const char* str) {
    ByteHelper<Command> cmd;
    if (parseEnum(&cmd, Command::CMD_MAX, str, "cmd")) {
        return cmd.operator Command();
    }
    return std::nullopt;
}

std::optional<data::WriteMsgToChatId> CommandParser::parseWriteMsg(char** argv) {
    data::WriteMsgToChatId data{};
    decltype(data.chat)::type chat;
    if (!try_parse(argv[0], &chat)) {
        return std::nullopt;
    }
    data.chat = chat;
    copyTo(data.message, argv[1]);
    return data;
}

std::optional<data::CtrlSpamBlock> CommandParser::parseCtrlSpamBlock(char** argv) {
    ByteHelper<data::CtrlSpamBlock> data;
    if (parseEnum(&data, data::CtrlSpamBlock::MAX, argv[0], "spamblock")) {
        return data;
    }
    return std::nullopt;
}

std::optional<data::ObserveChatId> CommandParser::parseObserveChat(char** argv) {
    data::ObserveChatId data{};
    decltype(data.chat)::type chat;
    if (try_parse(argv[0], &chat) && try_parse(argv[1], &data.observe)) {
        data.chat = chat;
        return data;
    }
    return std::nullopt;
}

std::optional<data::SendFileToChatId> CommandParser::parseSendFile(char** argv) {
    data::SendFileToChatId data{};
    ChatId id;
    ByteHelper<data::FileType> fileType;
    if (try_parse(argv[0], &id) &&
        parseEnum(&fileType, data::FileType::TYPE_MAX, argv[1], "type")) {
        data.chat = id;
        data.fileType = fileType;
        copyTo(data.filePath, argv[2]);
        return data;
    }
    return std::nullopt;
}

std::optional<data::ObserveAllChats> CommandParser::parseObserveAll(char** argv) {
    data::ObserveAllChats data{};
    bool observe = false;
    if (try_parse(argv[0], &observe)) {
        data.observe = observe;
        return data;
    }
    return std::nullopt;
}

std::optional<SocketFile2DataHelper::Params> CommandParser::parseTransferFile(char** argv) {
    SocketFile2DataHelper::Params params;
    params.filepath = argv[0];
    params.destfilepath = argv[1];
    params.options.hash_ignore = false;
    params.options.overwrite = true;
    params.options.dry_run = false;
    return params;
}

}  // namespace TgBotSocket::Client