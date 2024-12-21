#include <fmt/format.h>
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

Context::UDP::UDP(const boost::asio::ip::udp type, const int port)
    : socket_(io_context),
      endpoint_(boost::asio::ip::udp::endpoint(type, port)) {
    LOG(INFO) << fmt::format("UDP::UDP: Protocol {} listening on port {}", type,
                             port);
    _ioThread = std::thread([this]() {
        auto work = boost::asio::make_work_guard(io_context);
        io_context.run();
    });
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

bool Context::UDP::write(const SharedMalloc& data) const {
    try {
        socket_.send_to(boost::asio::buffer(data.get(), data.size()),
                        endpoint_);
        return true;
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "UDP::write: " << e.what();
        return false;
    }
}

std::optional<SharedMalloc> Context::UDP::read(
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

bool Context::UDP::close() const {
    boost::system::error_code ec;
    socket_.close(ec);  // NOLINT
    if (ec) {
        LOG(WARNING) << "Cannot close socket: " << ec.message();
    }
    return !ec;
}

bool Context::UDP::timeout(std::chrono::seconds time) {
    // Store the timeout duration for potential use in operations
    timeout_duration_ = time;
    return true;
}

bool Context::UDP::listen(listener_callback_t listener) {
    try {
        while (!io_context.stopped()) {
            auto data_opt = read(MAX_PACKET_SIZE);  // Maximum UDP packet size
            if (data_opt) {
                LOG(INFO) << "Received data from " << remoteAddress();
                listener(*this);
            }
        }
        return true;
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "UDP::listen: " << e.what();
        return false;
    }
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
    
    if (timeout_duration_.count() > 0) {
        // Wait for the connection to complete
        cv.wait_for(lk, timeout_duration_, [&] { return connected || io_context.stopped(); });
    } else {
        // Wait for the connection to complete
        cv.wait(lk, [&] { return connected || io_context.stopped(); });
    }

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
        return {.address=endpoint_.address().to_string(), .port=endpoint_.port()};
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "UDP::remoteAddress: " << e.what();
        return {};
    }
}

bool Context::UDP::abortConnections() { return close(); }

Context::UDP::operator bool() const { return socket_.is_open(); }

}  // namespace TgBotSocket
