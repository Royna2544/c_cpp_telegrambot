#include <fmt/format.h>

#include <boost/asio/use_future.hpp>
#include <boost/system/system_error.hpp>
#include <future>

#include "SocketContext.hpp"

template <>
struct fmt::formatter<boost::asio::ip::udp> : formatter<string_view> {
    // parse is inherited from formatter<string_view>.
    auto format(boost::asio::ip::udp c,
                format_context& ctx) const -> format_context::iterator {
        std::string_view name;
        if (c == boost::asio::ip::udp::v4()) {
            name = "IPv4";
        } else if (c == boost::asio::ip::udp::v6()) {
            name = "IPv6";
        } else {
            name = "Unknown";
        }
        return formatter<string_view>::format(name, ctx);
    }
};

namespace TgBotSocket {

Context::UDP::UDP(const boost::asio::ip::udp type, const uint_least16_t port)
    : socket_(io_context),
      endpoint_(boost::asio::ip::udp::endpoint(type, port)) {
    LOG(INFO) << fmt::format("UDP::UDP: Protocol {} listening on port {}", type,
                             port);
    _ioThread = std::thread([this]() {
        auto work = boost::asio::make_work_guard(io_context);
        io_context.run();
    });
    socket_.open(type);
}

Context::UDP::~UDP() {
    if (operator bool()) {
        close();
    }
    io_context.stop();
    if (_ioThread.joinable()) {
        _ioThread.join();
    }
}

bool Context::UDP::writeBlocking(const void* data, const size_t length) const {
    try {
        socket_.send_to(boost::asio::buffer(data, length), endpoint_);
        return true;
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "UDP::write: " << e.what();
        return false;
    }
}

bool Context::UDP::writeNonBlocking(const void* data,
                                    const size_t length) const {
    auto bytes_wrote = socket_.async_send_to(
        boost::asio::buffer(data, length), endpoint_, boost::asio::use_future);

    if (bytes_wrote.wait_until(std::chrono::system_clock::now() +
                               options.io_timeout.get()) !=
        std::future_status::ready) {
        LOG(ERROR) << "UDP::write: Timeout";
        return false;
    }

    try {
        return length == bytes_wrote.get();
    } catch (const boost::system::system_error& ex) {
        LOG(ERROR) << "UDP::read: Error reading: " << ex.what();
        return false;
    }
}

bool Context::UDP::write(const void* data, const size_t length) const {
    if (static_cast<bool>(options.io_timeout)) {
        return writeNonBlocking(data, length);
    } else {
        return writeBlocking(data, length);
    }
}

std::optional<SharedMalloc> Context::UDP::readNonBlocking(
    Packet::Header::length_type length) const {
    SharedMalloc buffer(length);
    boost::asio::ip::udp::endpoint sender_endpoint;
    auto bytes_received =
        socket_.async_receive_from(boost::asio::buffer(buffer.get(), length),
                                   sender_endpoint, boost::asio::use_future);

    if (bytes_received.wait_until(std::chrono::system_clock::now() +
                                  options.io_timeout.get()) !=
        std::future_status::ready) {
        LOG(ERROR) << "UDP::read: Timeout";
        return std::nullopt;
    }

    // Resize the buffer to the actual number of bytes received
    size_t bytes_read = 0;
    try {
        bytes_read = bytes_received.get();
    } catch (const boost::system::system_error& ex) {
        LOG(ERROR) << "UDP::read: Error reading: " << ex.what();
        return std::nullopt;
    }
    buffer.resize(bytes_read);
    // Update the endpoint to the sender's address
    endpoint_ = sender_endpoint;
    return buffer;
}

std::optional<SharedMalloc> Context::UDP::readBlocking(
    Packet::Header::length_type length) const {
    try {
        SharedMalloc buffer(length);
        boost::asio::ip::udp::endpoint sender_endpoint;
        size_t bytes_received = socket_.receive_from(
            boost::asio::buffer(buffer.get(), length), sender_endpoint);
        buffer.resize(bytes_received);
        // Update the endpoint to the sender's address
        endpoint_ = sender_endpoint;
        return buffer;
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "UDP::read: " << e.what();
        return std::nullopt;
    }
}

std::optional<SharedMalloc> Context::UDP::read(
    Packet::Header::length_type length) const {
    if (static_cast<bool>(options.io_timeout)) {
        return readNonBlocking(length);
    } else {
        return readBlocking(length);
    }
}

bool Context::UDP::close() const {
    boost::system::error_code ec;
    socket_.close(ec);  // NOLINT
    if (ec) {
        LOG(WARNING) << "Cannot close socket: " << ec.message();
    }
    return !ec;
}

bool Context::UDP::listen(listener_callback_t /*listener*/, bool block) {
    // This is actually a noop. UDP is connectionless and does not listen.
    LOG(WARNING) << "UDP::listen is a noop.";
    if (block) {
        io_context.run();
    }
    return true;
}

bool Context::UDP::connect(const RemoteEndpoint& endpoint) {
    // Resolve the address and port
    boost::asio::ip::udp::resolver resolver(io_context);
    auto endpoints =
        resolver.resolve(endpoint.address, std::to_string(endpoint.port));
    bool connected = false;

    // Create a condition variable to wait for the connection to complete
    std::condition_variable cv;
    std::mutex m;
    std::unique_lock<std::mutex> lk(m);

    // Attempt to connect to the resolved endpoints
    boost::asio::async_connect(
        socket_, endpoints,
        [&, this](boost::system::error_code ec,
                  const boost::asio::ip::udp::endpoint& /*ep*/) {
            if (ec) {
                LOG(ERROR) << "Connect error: " << ec.message();
            } else {
                connected = true;
            }
            cv.notify_all();
        });

    // Wait for the connection to complete
    cv.wait_for(lk, options.connect_timeout.get(),
                [&] { return connected || io_context.stopped(); });

    if (!connected) {
        LOG(ERROR) << "Failed to connect to " << endpoint;
        return false;
    }

    // Connection successful
    LOG(INFO) << "Connected to " << endpoint;
    return true;
}

Context::RemoteEndpoint Context::UDP::remoteAddress() const {
    try {
        return {.address = endpoint_.address().to_string(),
                .port = endpoint_.port()};
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "UDP::remoteAddress: " << e.what();
        return {};
    }
}

bool Context::UDP::abortConnections() { return close(); }

Context::UDP::operator bool() const { return socket_.is_open(); }

}  // namespace TgBotSocket
