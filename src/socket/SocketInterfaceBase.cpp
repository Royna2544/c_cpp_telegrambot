#include "SocketInterfaceBase.h"

#include "SingleThreadCtrl.h"
#include "socket/TgBotSocket.h"

bool SocketInterfaceBase::handleIncomingBuf(
    const ssize_t len, struct TgBotConnection& conn,
    const listener_callback_t& cb, std::function<char*(void)> errMsgFn) {
    if (len > 0) {
        if (conn.magic != MAGIC_VALUE) {
            LOG(WARNING) << "Invalid magic value, dropping buffer";
            return false;
        }

        if (TgBotCmd::isClientCommand(conn.cmd)) {
            LOG(INFO) << "Received buf with " << TgBotCmd::toStr(conn.cmd)
                      << ", invoke callback!";
        }

        if (conn.cmd == CMD_EXIT) {
            /**
             * @brief A static variable that stores the exit token.
             */
            static std::string exitToken;
            /**
             * @brief A static boolean variable that indicates whether the exit
             * token has been set or not.
             */
            static bool tokenSet = false;
            /**
             * @brief A string variable that stores the incoming exit token.
             */
            std::string exitToken_In = conn.data.data_2.token;

            switch (conn.data.data_2.op) {
                case ExitOp::SET_TOKEN:
                    /**
                     * @brief If the exit token has not been set, set it to the
                     * incoming token and return false to indicate that the
                     * incoming data was not handled completely.
                     */
                    if (!tokenSet) {
                        exitToken = exitToken_In;
                        tokenSet = true;
                        return false;
                    }
                    LOG(WARNING) << "Token was already set, but SET_TOKEN "
                                    "request, abort.";
                    break;
                case ExitOp::DO_EXIT:
                    /**
                     * @brief If the incoming exit token matches the stored exit
                     * token, exit the program. Otherwise, log a warning message
                     * indicating that different exit tokens were received.
                     */
                    if (exitToken != exitToken_In) {
                        LOG(WARNING)
                            << "Different exit tokens: My: '" << exitToken
                            << "', input: '" << exitToken_In << "'. Ignoring!";
                        return false;
                    }
                    break;
            }
            return true;
        }
        /**
         * @brief Invokes the listener_callback_t function with the given
         * TgBotConnection object.
         */
        cb(conn);
    } else {
        /**
         * @brief Logs an error message indicating that there was an error
         * reading from the socket.
         *
         * @param errMsg The error message returned by the error message
         * function.
         */
        LOG(ERROR) << "Failed to read from socket: " << errMsgFn();
    }
    return false;
}

void SocketInterfaceBase::stopListening(const std::string& exitToken) {
    if (canSocketBeClosed()) {
        writeToSocket({CMD_EXIT,
                       {.data_2 = TgBotCommandData::Exit::create(
                            ExitOp::DO_EXIT, exitToken)}});
    } else {
        forceStopListening();
    }
}

void SocketInterfaceBase::setOptions(Options opt, const std::string data,
                                     bool persistent) {
    option_t* optionVal = getOptionPtr(opt);
    if (optionVal != nullptr) {
        OptionContainer c;
        c.data = data;
        c.persistent = persistent;
        *optionVal = c;
    }
}

std::string SocketInterfaceBase::getOptions(Options opt) {
    std::string ret;
    option_t* optionVal = getOptionPtr(opt);
    if (optionVal) {
        option_t option = *optionVal;
        if (!option.has_value()) {
            LOG(FATAL) << "Option is not set, and trying to get it: option "
                       << static_cast<int>(opt);
        }
        ret = option->data;
        if (!option->persistent) option.reset();
    }
    return ret;
}

SocketInterfaceBase::option_t* SocketInterfaceBase::getOptionPtr(Options p) {
    option_t* optionVal = nullptr;
    switch (p) {
        case Options::DESTINATION_ADDRESS:
            optionVal = &options.opt_destination_address;
            break;
        case Options::DESTINATION_PORT:
            optionVal = &options.opt_destination_port;
            break;
    }
    return optionVal;
}

void SocketInterfaceBase::setupExitVerification() {
    writeToSocket({CMD_EXIT, {.data_2 = priv->e}});
}

void SocketInterfaceBase::runFunction() {
    startListening(priv->listener_callback, [this](const bool p) {
        if (p) {
            setupExitVerification();
            setPreStopFunction(
                [this](SingleThreadCtrl*) { stopListening(priv->e.token); });
        }
    });
}
