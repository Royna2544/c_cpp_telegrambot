#pragma once

#include <ApiDef.hpp>
#include <optional>
#include <string_view>

#include <bot/FileHelperNew.hpp>

namespace TgBotSocket::Client {

/**
 * @brief Parse command line arguments into command data structures
 */
class CommandParser {
public:
    /**
     * @brief Parse command enum from string
     */
    static std::optional<Command> parseCommand(const char* str);

    /**
     * @brief Parse arguments for CMD_WRITE_MSG_TO_CHAT_ID
     */
    static std::optional<data::WriteMsgToChatId> parseWriteMsg(char** argv);

    /**
     * @brief Parse arguments for CMD_CTRL_SPAMBLOCK
     */
    static std::optional<data::CtrlSpamBlock> parseCtrlSpamBlock(char** argv);

    /**
     * @brief Parse arguments for CMD_OBSERVE_CHAT_ID
     */
    static std::optional<data::ObserveChatId> parseObserveChat(char** argv);

    /**
     * @brief Parse arguments for CMD_SEND_FILE_TO_CHAT_ID
     */
    static std::optional<data::SendFileToChatId> parseSendFile(char** argv);

    /**
     * @brief Parse arguments for CMD_OBSERVE_ALL_CHATS
     */
    static std::optional<data::ObserveAllChats> parseObserveAll(char** argv);

    /**
     * @brief Parse arguments for CMD_TRANSFER_FILE
     */
    static std::optional<SocketFile2DataHelper::Params> parseTransferFile(char** argv);

private:
    template <class C>
    static bool parseEnum(ByteHelper<C>* res, C max, const char* str, std::string_view name);
};

}  // namespace TgBotSocket::Client