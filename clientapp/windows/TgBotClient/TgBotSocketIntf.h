
#pragma once

#include <functional>

// Include the tgbot's exported header
#include <filesystem>

#include <TgBotSocket_Export.hpp>

using namespace TgBotSocket;
using namespace TgBotSocket::data;
using namespace TgBotSocket::callback;

struct SocketConfig {
    std::string address;
    enum class Mode { USE_IPV4, USE_IPV6 } mode = Mode::USE_IPV4;
    int port;

    std::string_view ModetoStr() {
        switch (mode) {
            case Mode::USE_IPV4:
                return "IPv4";
            case Mode::USE_IPV6:
                return "IPv6";
        }
        return {};
    }
};

using GenericAckCallback = std::function<void(const GenericAck *)>;

extern bool sendMessageToChat(ChatId id, std::string message,
                              GenericAckCallback callback);
extern bool sendFileToChat(ChatId id, std::filesystem::path filepath,
                           FileType type, GenericAckCallback callback);
extern bool downloadFile(std::filesystem::path localDest, std::filesystem::path remoteSrc, 
    GenericAckCallback callback);
extern std::string getUptime();
extern void setSocketConfig(SocketConfig config);