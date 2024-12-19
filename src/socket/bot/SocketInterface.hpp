#include <ManagedThreads.hpp>
#include <ResourceManager.hpp>
#include <SocketContext.hpp>
#include <api/TgBotApi.hpp>
#include <global_handlers/ChatObserver.hpp>
#include <global_handlers/SpamBlock.hpp>
#include <stop_token>
#include <trivial_helpers/fruit_inject.hpp>

#include "FileHelperNew.hpp"
#include "TgBotSocket_Export.hpp"

using TgBotSocket::callback::GenericAck;

struct SocketInterfaceTgBot : ThreadRunner {
    void handlePacket(const TgBotSocket::Context& ctx, TgBotSocket::Packet pkt);

    void runFunction(const std::stop_token& token) override;
    void onPreStop() override;

    APPLE_EXPLICIT_INJECT(SocketInterfaceTgBot(
        TgBotSocket::Context* _interface, TgBotApi::Ptr _api,
        ChatObserver* observer, SpamBlockBase* spamblock,
        SocketFile2DataHelper* helper, ResourceProvider* resource));

   private:
    TgBotSocket::Context* _interface = nullptr;
    TgBotApi::Ptr api = nullptr;
    SocketFile2DataHelper* helper;
    ChatObserver* observer = nullptr;
    SpamBlockBase* spamblock = nullptr;
    ResourceProvider* resource = nullptr;

    std::chrono::system_clock::time_point startTp =
        std::chrono::system_clock::now();

    struct Session {
        std::string session_key;  // Randomly generated session key
        TgBotSocket::Packet::Header::nounce_type
            last_nonce;  // To prevent replay attacks
        std::chrono::system_clock::time_point expiry;  // Session expiry
    };

    std::unordered_map<std::string, Session> session_table;

    // Command handlers
    GenericAck handle_WriteMsgToChatId(
        const void* ptr, TgBotSocket::Packet::Header::length_type len,
        TgBotSocket::PayloadType type);
    GenericAck handle_SendFileToChatId(
        const void* ptr, TgBotSocket::Packet::Header::length_type len,
        TgBotSocket::PayloadType type);
    GenericAck handle_CtrlSpamBlock(const void* ptr);
    GenericAck handle_ObserveChatId(
        const void* ptr, TgBotSocket::Packet::Header::length_type len,
        TgBotSocket::PayloadType type);
    GenericAck handle_ObserveAllChats(
        const void* ptr, TgBotSocket::Packet::Header::length_type len,
        TgBotSocket::PayloadType type);
    GenericAck handle_TransferFile(const void* ptr,
                                   TgBotSocket::Packet::Header::length_type len,
                                   TgBotSocket::PayloadType type);

    // These have to create a packet to send back
    std::optional<TgBotSocket::Packet> handle_TransferFileRequest(
        const void* ptr, TgBotSocket::Packet::Header::length_type len,
        const TgBotSocket::Packet::Header::session_token_type& token,
        TgBotSocket::PayloadType type);

    std::optional<TgBotSocket::Packet> handle_GetUptime(
        const TgBotSocket::Packet::Header::session_token_type& token,
        TgBotSocket::PayloadType type);

    // Sessions
    void handle_OpenSession(const TgBotSocket::Context& ctx);
    void handle_CloseSession(
        const TgBotSocket::Packet::Header::session_token_type& token);

    bool verifyHeader(const TgBotSocket::Packet& packet);
};
