#include "SocketInterfaceBase.h"
#include <map>
#include <vector>
#include "socket/TgBotSocket.h"

bool SocketInterfaceBase::handleIncomingBuf(const size_t len, struct TgBotConnection& conn,
                                            const listener_callback_t& cb, std::function<char*(void)> errMsgFn) {
    if (len > 0) {
        if (conn.magic != MAGIC_VALUE) {
            LOG_W("Invalid magic value, dropping buffer");
            return false;
        }

        LOG_I("Received buf with %s, invoke callback!", TgBotCmd::toStr(conn.cmd).c_str());

        if (conn.cmd == CMD_EXIT) {
            /**
             * @brief A static variable that stores the exit token.
             */
            static std::string exitToken;
            /**
             * @brief A static boolean variable that indicates whether the exit token has been set or not.
             */
            static bool tokenSet = false;
            /**
             * @brief A string variable that stores the incoming exit token.
             */
            std::string exitToken_In = conn.data.data_2.token;

            switch (conn.data.data_2.op) {
                case ExitOp::SET_TOKEN:
                    /**
                     * @brief If the exit token has not been set, set it to the incoming token and return false
                     * to indicate that the incoming data was not handled completely.
                     */
                    if (!tokenSet) {
                        exitToken = exitToken_In;
                        tokenSet = true;
                        return false;
                    }
                    LOG_W("Token was already set, but SET_TOKEN request, abort.");
                    break;
                case ExitOp::DO_EXIT:
                    /**
                     * @brief If the incoming exit token matches the stored exit token, exit the program. Otherwise, log a warning message indicating that different exit tokens were received.
                     */
                    if (exitToken != exitToken_In) {
                        LOG_W("Different exit tokens: My: '%s', input: '%s'. Ignoring!", exitToken.c_str(), exitToken_In.c_str());
                        return false;
                    }
                    break;
            }
            return true;
        }
        /**
         * @brief Invokes the listener_callback_t function with the given TgBotConnection object.
         */
        cb(conn);
    } else {
        /**
         * @brief Logs an error message indicating that there was an error reading from the socket.
         *
         * @param errMsg The error message returned by the error message function.
         */
        LOG_E("Failed to read from socket: %s", errMsgFn());
    }
    return false;
}

void SocketInterfaceBase::stopListening(const std::string& exitToken) {
    if (canSocketBeClosed()) {
        writeToSocket({CMD_EXIT, {.data_2 = 
            TgBotCommandData::Exit::create(ExitOp::DO_EXIT, exitToken)}});
    } else {
        forceStopListening();
    }
}

void SocketInterfaceBase::setOptions(Options opt, const std::string data, bool persistent) {
    option_t *optionVal = nullptr;
    switch (opt) {
        case Options::DESTINATION_ADDRESS:
            optionVal = &options.opt_destination_address;
            break;
    }
    if (optionVal != nullptr) {
        OptionContainer c;
        c.data = data;
        c.persistent = persistent;
        *optionVal = c;
    }
}

std::string SocketInterfaceBase::getOptions(Options opt) {
    std::string ret;
    option_t *optionVal = nullptr;
 
    switch (opt) {
        case Options::DESTINATION_ADDRESS:
            optionVal = &options.opt_destination_address;
            break;
    }
    if (optionVal) {
        option_t option = *optionVal;
        ASSERT(option.has_value(), "Option value is not set, and trying to get, opt is %d", static_cast<int>(opt));
        ret = option->data;
        if (!option->persistent)
            option.reset();
    }
    return ret;
}

#if defined __linux__ || defined __APPLE__
#include "SocketInterfaceUnix.h"
#endif
#ifdef __WIN32
#include "SocketInterfaceWindows.h"
#endif

#define MAKE_INTF(usage, instance) {usage, instance}

std::shared_ptr<SocketInterfaceBase> getSocketInterface(const SocketUsage u) {
    static const std::map<SocketUsage, std::shared_ptr<SocketInterfaceBase>> socketBackends = {
#if defined __linux__ || defined __APPLE__
        MAKE_INTF(SocketUsage::SU_INTERNAL, std::make_shared<SocketInterfaceUnixLocal>()),
        MAKE_INTF(SocketUsage::SU_EXTERNAL, std::make_shared<SocketInterfaceUnixIPv4>())
#elif defined __WIN32
        MAKE_INTF(SocketUsage::SU_INTERNAL, std::make_shared<SocketInterfaceWindowsLocal>()),
        MAKE_INTF(SocketUsage::SU_EXTERNAL, std::make_shared<SocketInterfaceWindowsIPv4>()),
#endif
    };
    const auto it = socketBackends.find(u);
    if (it == socketBackends.end()) {
        return socketBackends.at(SocketUsage::SU_INTERNAL);
    }
    return it->second;
}
std::shared_ptr<SocketInterfaceBase> getSocketInterfaceForClient() {
    static const std::vector<std::shared_ptr<SocketInterfaceBase>> socketBackends = {
#if defined __linux__ || defined __APPLE__
        std::make_shared<SocketInterfaceUnixIPv4>(),
        std::make_shared<SocketInterfaceUnixIPv6>(),
        std::make_shared<SocketInterfaceUnixLocal>(),
#elif defined __WIN32
        std::make_shared<SocketInterfaceWindowsIPv4>(),
        std::make_shared<SocketInterfaceWindowsLocal>(),
#endif
    };
    for (const auto& socketBackend : socketBackends) { 
        if (socketBackend->isAvailable()) {
            return socketBackend;
        }
    }
    return {};
}
