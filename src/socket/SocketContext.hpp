#pragma once

#include <SocketExports.h>
#include <absl/log/log.h>

#include <ApiDef.hpp>
#include <boost/asio.hpp>
#include <chrono>
#include <filesystem>
#include <functional>
#include <optional>
#include <ostream>

#include "SharedMalloc.hpp"
#include "trivial_helpers/_class_helper_macros.h"
#include "trivial_helpers/generic_opt.hpp"

using std::chrono_literals::operator""s;

namespace TgBotSocket {

class SOCKET_EXPORT Context {
   public:
    Context();
    virtual ~Context();

    NO_COPY_CTOR(Context);

    using listener_callback_t = std::function<void(const Context &context)>;

    constexpr static int kTgBotHostPort = 50000;
    constexpr static int kTgBotLogPort = 50001;
    constexpr static int kTgBotLogTransmitPort = 50002;

    static std::filesystem::path hostPath() {
        return std::filesystem::temp_directory_path() / "tgbot.sock";
    }

    struct RemoteEndpoint {
        std::string address;
        uint_least16_t port;
    };

    // A specialized overload to write TgBotSocket::Packet.
    bool write(const TgBotSocket::Packet &packet) const;
    // Another specialized overload to write SharedMalloc.
    bool write(const SharedMalloc &data) const;

    /**
     * @brief Sends data over the socket connection.
     *
     * This function writes the specified data to the socket.
     *
     * @param data A pointer to the data to be sent.
     * @param length The number of bytes to send.
     */
    virtual bool write(const uint8_t *data, const size_t length) const = 0;

    /**
     * @brief Reads data from the socket connection.
     *
     * Attempts to read a specified number of bytes from the socket.
     *
     * @param length The number of bytes to read, specified by
     * `Packet::Header::length_type`.
     * @return An `std::optional<SharedMalloc>` containing the read data if
     * successful; `std::nullopt` if the read operation failed.
     */
    virtual std::optional<SharedMalloc> read(
        Packet::Header::length_type length) const = 0;

    /**
     * @brief Closes the socket connection.
     *
     * Closes the socket and releases any associated resources.
     *
     * @return `true` if the socket was successfully closed; otherwise, `false`.
     */
    virtual bool close() const = 0;

    /**
     * @brief Begins listening for incoming connections.
     *
     * Places the socket into a listening state, allowing it to accept incoming
     * connections.
     *
     * @param listener The callback function to be called when connection is
     * established.
     * @param block If `true`, the function will block.
     * The blocking behavior has advantage of additional thread launched.
     *
     * @return `true` if the socket successfully started listening; otherwise,
     * `false`.
     */
    virtual bool listen(listener_callback_t listener, bool block = false) = 0;

    /**
     * @brief Establishes a connection to a remote endpoint.
     *
     * @param endpoint The remote endpoint in `RemoteEndpoint' object.
     * @return `true` if the connection was successfully established; otherwise,
     * `false`.
     */
    virtual bool connect(const RemoteEndpoint &endpoint) = 0;

    /**
     * @brief Retrieves the remote address of the socket.
     *
     * Provides the IP address and port number of the remote endpoint as a
     * string.
     *
     * @return A `RemoteEndpoint` representing the remote address, for local
     * domain sockets, `port' is always set to 0.
     */
    virtual RemoteEndpoint remoteAddress() const = 0;

    /**
     * @brief Aborts all active connections.
     */
    virtual bool abortConnections() = 0;

    /**
     * @brief Checks if the socket is valid and operational.
     *
     * Allows the socket object to be used in boolean contexts to verify its
     * validity.
     *
     * @return `true` if the socket is valid and ready for operations;
     * otherwise, `false`.
     */
    virtual explicit operator bool() const = 0;

    // Classes implementing this interface
    class TCP;
    class UDP;
    class Local;

    // The I/O context.
    boost::asio::io_context io_context;

    struct {
        template <typename T>
        using Opt = generic_opt::Option<T>;
        Opt<std::chrono::seconds> connect_timeout{10};
        Opt<std::chrono::seconds> io_timeout;
    } options;

    // enum class Role { kNone, kServer, kClient } role;
};

extern std::ostream SOCKET_EXPORT &operator<<(
    std::ostream &stream, const Context::RemoteEndpoint &endpoint);

class SOCKET_EXPORT Context::TCP : public Context {
    mutable boost::asio::ip::tcp::socket socket_;
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::ip::tcp::endpoint endpoint_;
    bool is_listening_ = false;
    constexpr static std::size_t chunk_size = 1024;
    std::thread _ioThread;

    [[nodiscard]] std::optional<SharedMalloc> readNonBlocking(
        Packet::Header::length_type length) const;

    [[nodiscard]] std::optional<SharedMalloc> readBlocking(
        Packet::Header::length_type length) const;

    bool writeBlocking(const uint8_t *data, const size_t length) const;
    bool writeNonBlocking(const uint8_t *data, const size_t length) const;
   public:
    /**
     * @brief Construct TCP socket with a port.
     *
     * @param type TCP socket type, IPv4 or IPv6 socket (e.g.,
     * `boost::asio::ip::tcp::v4()`).
     * @param port The port to use.
     */
    explicit TCP(const boost::asio::ip::tcp type, const uint_least16_t port);

    // Virtual destructor.
    ~TCP() override;

    using Context::write;

    /**
     * @brief Sends data over the socket connection.
     *
     * This function writes the specified data to the socket.
     *
     * @return `true` if the data was successfully sent; otherwise, `false`.
     */
    bool write(const uint8_t *data, const size_t length) const override;

    /**
     * @brief Reads data from the socket connection.
     *
     * Attempts to read a specified number of bytes from the socket.
     *
     * @param length The number of bytes to read, specified by
     * `Packet::Header::length_type`.
     * @return An `std::optional<SharedMalloc>` containing the read data if
     * successful; `std::nullopt` if the read operation failed.
     */
    [[nodiscard]] std::optional<SharedMalloc> read(
        Packet::Header::length_type length) const override;

    /**
     * @brief Closes the socket connection.
     *
     * Closes the socket and releases any associated resources.
     *
     * @return `true` if the socket was successfully closed; otherwise, `false`.
     */
    bool close() const override;

    /**
     * @brief Begins listening for incoming connections.
     *
     * Places the socket into a listening state, allowing it to accept incoming
     * connections.
     *
     * @return `true` if the socket successfully started listening; otherwise,
     * `false`.
     */
    bool listen(listener_callback_t listener, bool block) override;

    /**
     * @brief Establishes a connection to a remote endpoint.
     *
     * @param endpoint The remote endpoint in `RemoteEndpoint' object.
     * @return `true` if the connection was successfully established; otherwise,
     * `false`.
     */
    bool connect(const RemoteEndpoint &endpoint) override;

    /**
     * @brief Retrieves the remote address of the socket.
     *
     * Provides the IP address and port number of the remote endpoint as a
     * string.
     *
     * @return A `RemoteEndpoint` representing the remote address, for local
     * domain sockets, `port' is always set to 0.
     */
    [[nodiscard]] RemoteEndpoint remoteAddress() const override;

    /**
     * @brief Aborts all active connections.
     */
    bool abortConnections() override;

    /**
     * @brief Checks if the socket is valid and operational.
     *
     * Allows the socket object to be used in boolean contexts to verify its
     * validity.
     *
     * @return `true` if the socket is valid and ready for operations;
     * otherwise, `false`.
     */
    explicit operator bool() const override;
};

class SOCKET_EXPORT Context::UDP : public Context {
    mutable boost::asio::ip::udp::socket socket_;
    mutable boost::asio::ip::udp::endpoint endpoint_;
    bool is_listening_ = false;
    std::thread _ioThread;
    constexpr static int MAX_PACKET_SIZE = 0x10000;

    [[nodiscard]] std::optional<SharedMalloc> readNonBlocking(
        Packet::Header::length_type length) const;

    [[nodiscard]] std::optional<SharedMalloc> readBlocking(
        Packet::Header::length_type length) const;

    bool writeBlocking(const uint8_t *data, const size_t length) const;
    bool writeNonBlocking(const uint8_t *data, const size_t length) const;

   public:
    /**
     * @brief Construct UDP socket with a port.
     *
     * @param type UDP socket type, IPv4 or IPv6 socket (e.g.,
     * `boost::asio::ip::udp::v4()`).
     * @param port The port to use.
     */
    explicit UDP(const boost::asio::ip::udp type, const uint_least16_t port);

    // Virtual destructor.
    ~UDP() override;

    using Context::write;

    /**
     * @brief Sends data over the socket connection.
     *
     * This function writes the specified data to the socket.
     *
     * @return `true` if the data was successfully sent; otherwise, `false`.
     */
    bool write(const uint8_t *data, const size_t length) const override;

    /**
     * @brief Reads data from the socket connection.
     *
     * Attempts to read a specified number of bytes from the socket.
     *
     * @param length The number of bytes to read, specified by
     * `Packet::Header::length_type`.
     * @return An `std::optional<SharedMalloc>` containing the read data if
     * successful; `std::nullopt` if the read operation failed.
     */
    [[nodiscard]] std::optional<SharedMalloc> read(
        Packet::Header::length_type length) const override;

    /**
     * @brief Closes the socket connection.
     *
     * Closes the socket and releases any associated resources.
     *
     * @return `true` if the socket was successfully closed; otherwise, `false`.
     */
    bool close() const override;

    /**
     * @brief Begins listening for incoming connections.
     *
     * Places the socket into a listening state, allowing it to accept incoming
     * connections.
     *
     * @return `true` if the socket successfully started listening; otherwise,
     * `false`.
     */
    bool listen(listener_callback_t listener, bool block) override;

    /**
     * @brief Establishes a connection to a remote endpoint.
     *
     * @param endpoint The remote endpoint in `RemoteEndpoint' object.
     * @return `true` if the connection was successfully established; otherwise,
     * `false`.
     */
    bool connect(const RemoteEndpoint &endpoint) override;

    /**
     * @brief Retrieves the remote address of the socket.
     *
     * Provides the IP address and port number of the remote endpoint as a
     * string.
     *
     * @return A `RemoteEndpoint` representing the remote address, for local
     * domain sockets, `port' is always set to 0.
     */
    [[nodiscard]] RemoteEndpoint remoteAddress() const override;

    /**
     * @brief Aborts all active connections.
     */
    bool abortConnections() override;

    /**
     * @brief Checks if the socket is valid and operational.
     *
     * Allows the socket object to be used in boolean contexts to verify its
     * validity.
     *
     * @return `true` if the socket is valid and ready for operations;
     * otherwise, `false`.
     */
    explicit operator bool() const override;
};

class SOCKET_EXPORT Context::Local : public Context {
    mutable boost::asio::local::stream_protocol::socket socket_;
    mutable boost::asio::local::stream_protocol::acceptor acceptor_;
    boost::asio::local::stream_protocol::endpoint endpoint_;
    bool is_listening_ = false;
    std::thread _ioThread;

    [[nodiscard]] std::optional<SharedMalloc> readNonBlocking(
        Packet::Header::length_type length) const;

    [[nodiscard]] std::optional<SharedMalloc> readBlocking(
        Packet::Header::length_type length) const;

    bool writeBlocking(const uint8_t *data, const size_t length) const;
    bool writeNonBlocking(const uint8_t *data, const size_t length) const;

   public:
    /**
     * @brief Construct Local socket with a path. (Server)
     *
     * @param path The path to the Unix domain socket.
     */
    explicit Local(const std::filesystem::path &path);

    /**
     * @brief Initalize local socket client.
     *
     * @param path The path to the Unix domain socket.
     */
    explicit Local();

    // Virtual destructor.
    ~Local() override;

    using Context::write;

    /**
     * @brief Sends data over the socket connection.
     *
     * This function writes the specified data to the socket.
     *
     * @return `true` if the data was successfully sent; otherwise, `false`.
     */
    bool write(const uint8_t *data, const size_t length) const override;

    /**
     * @brief Reads data from the socket connection.
     *
     * Attempts to read a specified number of bytes from the socket.
     *
     * @param length The number of bytes to read, specified by
     * `Packet::Header::length_type`.
     * @return An `std::optional<SharedMalloc>` containing the read data if
     * successful; `std::nullopt` if the read operation failed.
     */
    [[nodiscard]] std::optional<SharedMalloc> read(
        Packet::Header::length_type length) const override;

    /**
     * @brief Closes the socket connection.
     *
     * Closes the socket and releases any associated resources.
     *
     * @return `true` if the socket was successfully closed; otherwise, `false`.
     */
    bool close() const override;

    /**
     * @brief Begins listening for incoming connections.
     *
     * Places the socket into a listening state, allowing it to accept incoming
     * connections.
     *
     * @return `true` if the socket successfully started listening; otherwise,
     * `false`.
     */
    bool listen(listener_callback_t listener, bool block) override;

    /**
     * @brief Establishes a connection to a remote endpoint.
     *
     * @param endpoint The remote endpoint in `RemoteEndpoint' object.
     * @return `true` if the connection was successfully established; otherwise,
     * `false`.
     */
    bool connect(const RemoteEndpoint &endpoint) override;

    /**
     * @brief Retrieves the remote address of the socket.
     *
     * Provides the IP address and port number of the remote endpoint as a
     * string.
     *
     * @return A `RemoteEndpoint` representing the remote address, for local
     * domain sockets, `port' is always set to 0.
     */
    [[nodiscard]] RemoteEndpoint remoteAddress() const override;

    /**
     * @brief Aborts all active connections.
     */
    bool abortConnections() override;

    /**
     * @brief Checks if the socket is valid and operational.
     *
     * Allows the socket object to be used in boolean contexts to verify its
     * validity.
     *
     * @return `true` if the socket is valid and ready for operations;
     * otherwise, `false`.
     */
    explicit operator bool() const override;
};

}  // namespace TgBotSocket
