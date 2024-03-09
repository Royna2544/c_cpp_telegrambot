#pragma once

#include <future>

#include "TgBotSocket.h"

using listener_callback_t = std::function<void(struct TgBotConnection)>;

struct SocketInterfaceBase {
    /**
     * @brief Writes a TgBotConnection to the socket.
     *
     * @param conn The TgBotConnection to write to the socket.
     */
    virtual void writeToSocket(struct TgBotConnection conn) = 0;

    /**
     * @brief Stops the socket listener thread.
     *
     * This function will cause the socket listener thread to exit immediately.
     * It is intended to be used in cases where the program needs to terminate
     * immediately, and the normal shutdown process is not sufficient.
     */
    virtual void forceStopListening(void) = 0;

    /**
     * @brief Starts the socket listener thread.
     *
     * @param cb The function to be called when a new connection is received.
     * @param createdProm A promise that will be fulfilled with true when the listener is successfully created, or false if it fails.
     */
    virtual void startListening(const listener_callback_t& cb, std::promise<bool>& createdPromise) = 0;
    
    // TODO - IPV4 requires this, but not all does
    virtual void setDestinationAddress(const std::string addr = "") {}

    // Can this instance exit cleanly with CMD_EXIT?
    virtual bool canSocketBeClosed() { return true; }
    
    virtual ~SocketInterfaceBase() = default;

    void stopListening(const std::string& exittoken);

    bool isRunning = false;
   protected:
    /**
     * @brief This function is used to handle incoming data from the Telegram Bot API.
     *
     * @param len The length of the incoming data.
     * @param conn The TgBotConnection object that contains information about the incoming data.
     * @param cb The listener_callback_t function that will be invoked when the incoming data is processed.
     * @param errMsgFn The function that returns an error message when there is an error reading from the socket.
     *
     * @return true if the incoming data was handled successfully, false otherwise.
     */
    [[nodiscard]] bool handleIncomingBuf(const size_t len, struct TgBotConnection& conn,
                                         const listener_callback_t& cb, std::function<char*(void)> errMsgFn);
};

enum SocketUsage {
    SU_INTERNAL, // Inside bot scope only, like cmd_exit
    SU_EXTERNAL, // Sent through clients, like socketcli, mediacli
    SU_MAX,
};

std::shared_ptr<SocketInterfaceBase> getSocketInterface(const SocketUsage usage);
