#pragma once

#include <SocketDescriptor_defs.hpp>
#include <functional>
#include <optional>
#include <string>

#include "SharedMalloc.hpp"
#include <socket/TgBotSocket.h>

struct SocketConnContext {
    socket_handle_t cfd{};  // connection socket file descriptor
    SharedMalloc addr;      // struct sockaddr_*'s address

    template <typename SocketAddr>
    static SocketConnContext create() {
        SocketAddr addr{};
        SocketConnContext ctx(addr);
        return ctx;
    }
    template <typename SocketAddr>
    explicit SocketConnContext(SocketAddr Myaddr) : addr(Myaddr) {}
};

// A base class for socket operations
struct SocketInterfaceBase {
    enum class Options { DESTINATION_ADDRESS, DESTINATION_PORT };

    // addr is used as void pointer to maintain platform independence.
    using listener_callback_t = std::function<bool(SocketConnContext ctx)>;
    using dummy_listen_buf_t = char;

    constexpr static int kTgBotHostPort = 50000;

    void writeAsClientToSocket(SharedMalloc data);
    void startListeningAsServer(const listener_callback_t onNewData);
    bool closeSocketHandle(SocketConnContext &context);

    /**
     * @brief Writes data to the socket using the provided context.
     *
     * This function is a pure virtual function, meaning it must be implemented
     * by any class that inherits from SocketInterfaceBase. It is used to send
     * data over the network using the specified connection context.
     *
     * @param context The connection context containing the socket file
     * descriptor, address, and length of the address of the destination.
     * @param data The data to be written to the socket.
     */
    virtual void writeToSocket(SocketConnContext context,
                               SharedMalloc data) = 0;

    /**
     * @brief Reads data from the socket using the provided context.
     *
     * This function is a pure virtual function, meaning it must be implemented
     * by any class that inherits from SocketInterfaceBase. It is used to
     * receive data from the network using the specified connection context.
     *
     * @param context The connection context containing the socket file
     * descriptor, address, and length of the address of the source.
     * @param length The maximum length of data to be read from the socket.
     *
     * @return An optional containing the received data if successful, or an
     * empty optional if an error occurred or no data was available.
     */
    virtual std::optional<SharedMalloc> readFromSocket(
        SocketConnContext context,
        TgBotCommandPacketHeader::length_type length) = 0;

    /**
     * @brief Closes the socket handle.
     *
     * This function is a pure virtual function, meaning it must be implemented
     * by any class that inherits from SocketInterfaceBase. It is used to close
     * the specified socket handle and release any system resources associated
     * with it.
     * Also, this function should invalidate the socket handle
     *
     * @param handle The socket handle to be closed.
     *
     * @return True if the socket handle was successfully closed, false
     * otherwise. Note: The actual return value may vary depending on the
     * specific implementation.
     */
    virtual bool closeSocketHandle(socket_handle_t &handle) = 0;

    /**
     * @brief Starts the socket listener thread.
     *
     * @param handle The socket handle.
     * @param onNewBuffer The function to be called when a new connection
     * is received.
     */
    virtual void startListening(socket_handle_t handle,
                                listener_callback_t onNewBuffer) = 0;

    /**
     * @brief Creates a new client socket.
     *
     * This function is used to create a new client socket. The specific
     * implementation of this function will depend on the underlying socket
     * interface being used.
     *
     * @return A SocketConnContext object containing the connection socket
     * file descriptor, address, and length of the address.
     * Contains the socket handle of the server, server's address and length
     */
    virtual std::optional<SocketConnContext> createClientSocket() = 0;

    /**
     * @brief Creates a new server socket.
     *
     * This function is used to create a new server socket. The specific
     * implementation of this function will depend on the underlying socket
     * interface being used.
     *
     * @return A socket handle representing the newly created server socket.
     */
    virtual std::optional<socket_handle_t> createServerSocket() = 0;

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
    virtual bool isSupported() = 0;

    struct Helper {
        explicit Helper(SocketInterfaceBase *interface_)
            : inet(interface_), local(interface_) {}

        struct INetHelper {
            explicit INetHelper(SocketInterfaceBase *interface_)
                : interface(interface_) {}
            bool isSupportedIPv4(void);
            bool isSupportedIPv6(void);
            int getPortNum();
            std::string getExternalIP(void);
            static size_t externalIPCallback(void *contents, size_t size,
                                             size_t nmemb, void *userp);

           private:
            constexpr static std::string_view kIPv4EnvVar = "IPV4_ADDRESS";
            constexpr static std::string_view kIPv6EnvVar = "IPV6_ADDRESS";
            constexpr static std::string_view kPortEnvVar = "PORT_NUM";
            bool _isSupported(const std::string_view envVar);
            SocketInterfaceBase *interface;
        } inet;

        struct LocalHelper {
            explicit LocalHelper(SocketInterfaceBase *interface_)
                : interface(interface_) {}
            static bool isSupported(void);
            bool canSocketBeClosed();
            void cleanupServerSocket();
            void doGetRemoteAddr(socket_handle_t handle);

           private:
            SocketInterfaceBase *interface;
        } local;
    } helper;

    // Declare aliases
    using INetHelper = Helper::INetHelper;
    using LocalHelper = Helper::LocalHelper;

    SocketInterfaceBase() : helper(this) {}
    virtual ~SocketInterfaceBase() = default;

    /**
     * @brief Stops the socket listener thread.
     *
     * This function will cause the socket listener thread to exit immediately.
     * It is intended to be used in cases where the program needs to terminate
     * immediately, and the normal shutdown process is not sufficient.
     */
    virtual void forceStopListening(void) = 0;

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
