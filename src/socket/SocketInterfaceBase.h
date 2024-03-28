#pragma once

#include <memory>
#include <optional>

#include "../include/SingleThreadCtrl.h"
#include "TgBotSocket.h"

using listener_callback_t = std::function<void(struct TgBotConnection)>;
using result_callback_t = std::function<void(const bool)>;
struct SocketInterfacePriv {
    listener_callback_t listener_callback;
    TgBotCommandData::Exit e;
};
struct SocketInterfaceBase : SingleThreadCtrlRunnable<SocketInterfacePriv> {
    enum class Options { DESTINATION_ADDRESS, DESTINATION_PORT };

    /**
     * @brief Writes a TgBotConnection to the socket.
     *
     * @param conn The TgBotConnection to write to the socket.
     */
    virtual void writeToSocket(struct TgBotConnection conn) = 0;

    /**
     * @brief Starts the socket listener thread.
     *
     * @param listener_callback The function to be called when a new connection
     * is received.
     * @param result_callback A function that will be called when the socket
     * listener thread has started.
     */
    virtual void startListening(const listener_callback_t &listener_callback,
                                const result_callback_t &result_callback) = 0;

    /**
     * @brief Sets an option for the socket interface.
     *
     * @param opt The option to set.
     * @param data The data to set the option to.
     * @param persistent Whether or not the option should be persisted across
     * restarts.
     */
    void setOptions(Options opt, const std::string data,
                    bool persistent = false);

    /**
     * @brief Returns the value of an option for the socket interface.
     *
     * @param opt The option to retrieve the value of.
     *
     * @return The value of the option
     * @throws std::bad_optional_access if the option does not exist.
     */
    std::string getOptions(Options opt);

    /**
     * @brief Indicates whether the socket interface is available.
     *
     * A socket interface is considered available if it can be used to listen
     * for incoming connections. This function should be used to determine
     * whether the socket interface is ready to be used. (Or its dependencies
     * are available)
     *
     * @return true if the socket interface is available, false otherwise.
     */
    virtual bool isAvailable() = 0;

    virtual ~SocketInterfaceBase() = default;

    // Stop listening with exit token
    virtual void stopListening(const std::string &exittoken);

    void runFunction() override;

    using dummy_listen_buf_t = char;

    constexpr static int kTgBotHostPort = 50000;

   protected:
    /**
     * @brief Cleans up the server socket.
     *
     * This function is called when the server socket is no longer needed.
     * It will close the socket and free any system resources associated with
     * it.
     */
    virtual void cleanupServerSocket() {}

    // Setup exit verification via token
    virtual void setupExitVerification();

    // Can this instance exit cleanly with CMD_EXIT?
    virtual bool canSocketBeClosed() { return true; }

    /**
     * @brief Stops the socket listener thread.
     *
     * This function will cause the socket listener thread to exit immediately.
     * It is intended to be used in cases where the program needs to terminate
     * immediately, and the normal shutdown process is not sufficient.
     */
    virtual void forceStopListening(void) = 0;

    /**
     * @brief This function is used to handle incoming data from the Telegram
     * Bot API.
     *
     * @param len The length of the incoming data.
     * @param conn The TgBotConnection object that contains information about
     * the incoming data.
     * @param cb The listener_callback_t function that will be invoked when the
     * incoming data is processed.
     * @param errMsgFn The function that returns an error message when there is
     * an error reading from the socket.
     *
     * @return true if the incoming data was handled successfully, false
     * otherwise.
     */
    [[nodiscard]] bool handleIncomingBuf(const size_t len,
                                         struct TgBotConnection &conn,
                                         const listener_callback_t &cb,
                                         std::function<char *(void)> errMsgFn);

    struct OptionContainer {
        std::string data;
        bool persistent = false;
    };
    using option_t = std::optional<OptionContainer>;
    struct {
        option_t opt_destination_address;
        option_t opt_destination_port;
    } options;

   private:
    option_t *getOptionPtr(Options opt);
};

struct SocketHelperCommon {
    static bool isAvailableIPv4(SocketInterfaceBase *it);
    static bool isAvailableIPv6(SocketInterfaceBase *it);
    static bool isAvailableLocalSocket(void);
    static int getPortNumInet(SocketInterfaceBase *it);
    static bool canSocketBeClosedLocalSocket(SocketInterfaceBase *it);
    static void cleanupServerSocketLocalSocket(SocketInterfaceBase *it);
    static void printExternalIPINet(void);

   private:
    constexpr static const char kIPv4EnvVar[] = "IPV4_ADDRESS";
    constexpr static const char kIPv6EnvVar[] = "IPV6_ADDRESS";
    constexpr static const char kPortEnvVar[] = "PORT_NUM";
    static bool _isAvailable(SocketInterfaceBase *it, const char *envVar);
    static size_t externalIPCallback(void *contents, size_t size, size_t nmemb,
                                     void *userp);
};