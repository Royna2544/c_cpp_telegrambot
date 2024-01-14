#include <chrono>

#include "TgBotSocket.h"

static constexpr inline auto sleep_sec = std::chrono::seconds(4);

[[nodiscard]] static inline bool handleIncomingBuf(const size_t len, struct TgBotConnection& conn,
                                                   const listener_callback_t& cb, char* errbuf) {
    if (len > 0) {
        if (conn.cmd == CMD_EXIT) {
            LOG_D("Received exit command");
            return true;
        }
        LOG_D("Received buf with %s, invoke callback!", toStr(conn.cmd).c_str());
        cb(conn);
    } else {
        LOG_E("Failed to read from socket: %s", errbuf);
    }
    return false;
}