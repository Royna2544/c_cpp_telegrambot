#pragma once

#include <SocketData.hpp>
#include <SocketDescriptor_defs.hpp>
#include <functional>
#include <optional>
#include <string>

// A base class for socket operations
struct SocketInterfaceBase {
    enum class Options { DESTINATION_ADDRESS, DESTINATION_PORT };

    using listener_callback_t =
        std::function<bool(SocketInterfaceBase *intf, socket_handle_t cfd)>;
    using dummy_listen_buf_t = char;

    constexpr static int kTgBotHostPort = 50000;

    /**
     * @brief Writes a SocketData to the socket.
     *
     * @param data The SocketData to write to the socket.
     */
    virtual void writeToSocket(struct SocketData data) = 0;

    /**
     * @brief Starts the socket listener thread.
     *
     * @param onNewBuffer The function to be called when a new connection
     * is received.
     */
    virtual void startListening(listener_callback_t onNewBuffer) = 0;

    /**
     * @brief Creates a new client socket.
     *
     * @return A new socket handle, or INVALID_SOCKET on error.
     */
    virtual socket_handle_t createClientSocket() = 0;

    /**
     * @brief Creates a new server socket.
     *
     * @return A new socket handle, or INVALID_SOCKET on error.
     */
    virtual socket_handle_t createServerSocket() = 0;

    /**
     * @brief Retrieves the remote address of a connected socket.
     *
     * @param handle The socket handle.
     */
    virtual void doGetRemoteAddr(socket_handle_t handle) = 0;

    /**
     * @brief Checks if a socket handle is valid.
     *
     * @param handle The socket handle.
     * @return True if the handle is valid, false otherwise.
     */
    virtual bool isValidSocketHandle(socket_handle_t handle) = 0;

    /**
     * @brief read data from socket handle, wrapper for os-specific read()
     * function
     *
     * @param handle socket handle object passed from listener_callback function
     * @param length length of data to read
     */
    virtual std::optional<SocketData> readFromSocket(socket_handle_t handle,
                                      SocketData::length_type length) = 0;

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

    struct Helper {
        explicit Helper(SocketInterfaceBase *interface_)
            : inet(interface_), local(interface_) {}

        struct INetHelper {
            explicit INetHelper(SocketInterfaceBase *interface_)
                : interface(interface_) {}
            bool isAvailableIPv4(void);
            bool isAvailableIPv6(void);
            int getPortNum();
            void printExternalIP(void);
            static size_t externalIPCallback(void *contents, size_t size,
                                             size_t nmemb, void *userp);

           private:
            constexpr static const char kIPv4EnvVar[] = "IPV4_ADDRESS";
            constexpr static const char kIPv6EnvVar[] = "IPV6_ADDRESS";
            constexpr static const char kPortEnvVar[] = "PORT_NUM";
            bool _isAvailable(const char *envVar);
            SocketInterfaceBase *interface;
        } inet;

        struct LocalHelper {
            explicit LocalHelper(SocketInterfaceBase *interface_)
                : interface(interface_) {}
            static bool isAvailable(void);
            bool canSocketBeClosed();
            void cleanupServerSocket();

           private:
            SocketInterfaceBase *interface;
        } local;
    } helper;

    // Declare aliases
    using INetHelper = Helper::INetHelper;
    using LocalHelper = Helper::LocalHelper;

    SocketInterfaceBase() : helper(this) {}
    virtual ~SocketInterfaceBase() = default;

   protected:
    /**
     * @brief Cleans up the server socket.
     *
     * This function is called when the server socket is no longer needed.
     * It will close the socket and free any system resources associated with
     * it.
     */
    virtual void cleanupServerSocket() {}

    /**
     * @brief Checks if the server socket can be written currently.
     * This isn't really useful on inet(4/6), but local socket is file based.
     *
     * @return true if the server can accept connections.
     */
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
     * @brief Returns the last error message associated with the socket
     * interface.
     *
     * @return A pointer to the last error message.
     */
    virtual char *getLastErrorMessage() = 0;

   private:
    struct OptionContainer {
        std::string data;
        bool persistent = false;
    };
    using option_t = std::optional<OptionContainer>;
    struct {
        option_t opt_destination_address;
        option_t opt_destination_port;
    } options;

    option_t *getOptionPtr(Options opt);
};
