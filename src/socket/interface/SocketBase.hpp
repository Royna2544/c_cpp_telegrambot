#pragma once

#include <absl/log/log.h>

#include <SocketDescriptor_defs.hpp>
#include <TgBotSocket_Export.hpp>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>

using std::chrono_literals::operator""s;

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

    template <typename SocketAddr>
    explicit SocketConnContext(socket_handle_t sock, SocketAddr Myaddr)
        : SocketConnContext(Myaddr) {
        cfd = sock;
    }
    bool operator!=(const SocketConnContext &other) const noexcept {
        return !(*this == other);
    }
    bool operator==(const SocketConnContext &other) const noexcept {
        return cfd == other.cfd && addr == other.addr;
    }
};

// A base class for socket operations
struct SocketInterfaceBase {
    // addr is used as void pointer to maintain platform independence.
    using listener_callback_t = std::function<bool(SocketConnContext ctx)>;
    using dummy_listen_buf_t = char;
    using buffer_len_t = TgBotSocket::PacketHeader::length_type;

    constexpr static int kTgBotHostPort = 50000;
    constexpr static int kTgBotLogPort = 50001;

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
    virtual bool writeToSocket(SocketConnContext context,
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
        SocketConnContext context, buffer_len_t length) = 0;

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
     * @brief Sets the timeout for socket operations.
     *
     * This function is a pure virtual function, meaning it must be implemented
     * by any class that inherits from SocketInterfaceBase. It is used to set
     * the timeout for socket operations, such as read and write, to prevent the
     * program from hanging indefinitely in case of network issues.
     *
     * @param handle The socket handle.
     * @param timeout The timeout value in seconds. A value of 0 means no
     * timeout, and a negative value means to use the system default timeout.
     *
     * @return True if the timeout was successfully set, false otherwise. Note:
     * The actual return value may vary depending on the specific
     * implementation.
     */
    virtual bool setSocketOptTimeout(socket_handle_t handle, int timeout) = 0;

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
    virtual void printRemoteAddress(socket_handle_t handle) = 0;

    /**
     * @brief Checks if a socket handle is valid.
     *
     * @param handle The socket handle.
     * @return True if the handle is valid, false otherwise.
     */
    virtual bool isValidSocketHandle(socket_handle_t handle) = 0;

    struct Helper {
        explicit Helper(SocketInterfaceBase *interface_)
            : inet(interface_), local(interface_) {}

        struct INetHelper {
            explicit INetHelper(SocketInterfaceBase *interface_)
                : interface(interface_) {}
            int getPortNum();
            std::string getExternalIP(void);
            static size_t externalIPCallback(void *contents, size_t size,
                                             size_t nmemb, void *userp);

           private:
            SocketInterfaceBase *interface;
        } inet;

        struct LocalHelper {
            explicit LocalHelper(SocketInterfaceBase *interface_)
                : interface(interface_) {}
            bool canSocketBeClosed();
            void cleanupServerSocket();
            static void printRemoteAddress(socket_handle_t handle);
            static std::filesystem::path getSocketPath();

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

    // A generic template struct to hold optional data and a persistent flag
    template <typename T>
    struct Option {
        // std::optional to hold the data
        std::optional<T> data;

        // Default constructor
        Option() = default;

        // Option with default value
        explicit Option(T defaultValue) : data(defaultValue) {}

        // Function to set the data
        void set(T dataIn) { data = dataIn; }

        // Function to get the data and reset it if not persistent
        [[nodiscard]] T get() const {
            // Specially for bool types.
            if constexpr (std::is_same_v<T, bool>) {
                return data.value_or(false);
            }
            if (!operator bool()) {
                LOG(WARNING) << "Trying to get data which is not set!";
            }
            // Throws std::bad_optional_access if not set
            return data.value();
        }

        Option &operator=(const T &other) {
            set(other);
            return *this;
        }

        // Explicit conversion operator to check if data is present
        explicit operator bool() const { return data.has_value(); }
    };

    // A nested struct to hold optional parameters for the socket operations
    struct {
        // Option to set the address for socket operations
        Option<std::string> address;
        // Option to set the port for socket operations
        Option<int> port;
        // Option to specify whether to use UDP for socket operations
        Option<bool> use_udp;
        // Option to specify whether to use connection timeouts for client
        Option<bool> use_connect_timeout;
        // Option to specify the timeout for socket operations
        // Used if use_connect_timeout is true
        Option<std::chrono::seconds> connect_timeout{10s};
    } options;

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
};
