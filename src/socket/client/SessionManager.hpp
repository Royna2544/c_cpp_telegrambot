#pragma once

#include <ApiDef.hpp>
#include <ClientBackend.hpp>
#include <optional>

namespace TgBotSocket::Client {

/**
 * @brief Manage client session lifecycle
 */
class SessionManager {
public:
    explicit SessionManager(SocketClientWrapper& backend);

    /**
     * @brief Open a new session with the server
     * @return Session token if successful
     */
    std::optional<Packet::Header::session_token_type> openSession();

    /**
     * @brief Close the current session
     * @param token Session token to close
     * @return true if successful
     */
    bool closeSession(const Packet::Header::session_token_type& token);

private:
    SocketClientWrapper& backend_;
};

}  // namespace TgBotSocket::Client