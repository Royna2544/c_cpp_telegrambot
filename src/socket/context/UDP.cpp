#include <fmt/format.h>

#include <array>
#include <memory>
#include <boost/asio/use_future.hpp>
#include <boost/system/system_error.hpp>
#include <future>

#include "../SocketContext.hpp"

template <>
struct fmt::formatter<boost::asio::ip::udp> : formatter<string_view> {
    // parse is inherited from formatter<string_view>.
    auto format(boost::asio::ip::udp c, format_context& ctx) const
        -> format_context::iterator {
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
    work_guard_.emplace(boost::asio::make_work_guard(io_context));
    _ioThread = std::thread([this]() {
        io_context.run();
    });
    socket_.open(type);
}

Context::UDP::~UDP() {
    if (operator bool()) {
        close();
    }
    // Reset the work guard to allow io_context to finish
    work_guard_.reset();
    io_context.stop();
    if (_ioThread.joinable()) {
        _ioThread.join();
    }
}

bool Context::UDP::writeBlocking(const uint8_t* data,
                                 const size_t length) const {
    try {
        std::lock_guard<std::mutex> lock(endpoint_mutex_);
        socket_.send_to(boost::asio::buffer(data, length), endpoint_);
        return true;
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "UDP::write: " << e.what();
        return false;
    }
}

bool Context::UDP::writeNonBlocking(const uint8_t* data,
                                    const size_t length) const {
    boost::asio::ip::udp::endpoint ep;
    {
        std::lock_guard<std::mutex> lock(endpoint_mutex_);
        ep = endpoint_;
    }
    auto bytes_wrote = socket_.async_send_to(
        boost::asio::buffer(data, length), ep, boost::asio::use_future);

    if (bytes_wrote.wait_until(std::chrono::system_clock::now() +
                               options.io_timeout.get()) !=
        std::future_status::ready) {
        LOG(ERROR) << "UDP::write: Timeout";
        socket_.cancel();
        return false;
    }

    try {
        return length == bytes_wrote.get();
    } catch (const boost::system::system_error& ex) {
        LOG(ERROR) << "UDP::read: Error reading: " << ex.what();
        return false;
    }
}

bool Context::UDP::write(const uint8_t* data, const size_t length) const {
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
        socket_.cancel();
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
    {
        std::lock_guard<std::mutex> lock(endpoint_mutex_);
        endpoint_ = sender_endpoint;
    }
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
        {
            std::lock_guard<std::mutex> lock(endpoint_mutex_);
            endpoint_ = sender_endpoint;
        }
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
    socket_.cancel();
    socket_.close(ec);  // NOLINT
    if (ec) {
        LOG(WARNING) << "Cannot close socket: " << ec.message();
    }
    return !ec;
}

bool Context::UDP::listen(listener_callback_t listener, bool block) {
    // For UDP, we wait for the first packet to establish the endpoint
    // Then call the listener callback to start the logging sink
    LOG(INFO) << "UDP::listen: Waiting for first packet to establish endpoint";
    
    // Bind the socket to the endpoint (server side only)
    try {
        socket_.bind(endpoint_);
        LOG(INFO) << "UDP::listen: Bound to " << endpoint_.address().to_string() 
                  << ":" << endpoint_.port();
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "UDP::listen: Failed to bind socket: " << e.what();
        return false;
    }
    
    // Start an async receive operation to wait for the first packet
    // Use a shared pointer for the endpoint to keep it alive
    auto sender_endpoint = std::make_shared<boost::asio::ip::udp::endpoint>();
    auto buffer = std::make_shared<std::array<uint8_t, 1>>();
    
    socket_.async_receive_from(
        boost::asio::buffer(*buffer), *sender_endpoint,
        [this, listener, sender_endpoint](boost::system::error_code ec, std::size_t /*bytes_received*/) {
            if (ec) {
                LOG(ERROR) << "UDP::listen: Error receiving first packet: " << ec.message();
                return;
            }
            
            // Update endpoint to the client's address
            {
                std::lock_guard<std::mutex> lock(endpoint_mutex_);
                endpoint_ = *sender_endpoint;
            }
            LOG(INFO) << "UDP::listen: Received first packet from " << remoteAddress();
            
            // Call the listener callback to start the logging sink
            listener(*this);
        });
    
    // Note: Don't call io_context.run() here as it's already running in the constructor's thread
    // The block parameter is ignored for UDP as the io_context is managed by the constructor
    if (block) {
        LOG(WARNING) << "UDP::listen: block=true is ignored; io_context already running in background";
    }
    return true;
}

bool Context::UDP::connect(const RemoteEndpoint& endpoint) {
    try {
        // Resolve the address and port
        boost::asio::ip::udp::resolver resolver(io_context);
        auto resolved_endpoints =
            resolver.resolve(endpoint.address, std::to_string(endpoint.port));
        
        // For UDP, we use socket.connect() to set the default remote endpoint
        // This is different from TCP - it doesn't establish a connection
        if (resolved_endpoints.empty()) {
            LOG(ERROR) << "Failed to resolve endpoint: " << endpoint;
            return false;
        }
        
        // Use the first resolved endpoint
        {
            std::lock_guard<std::mutex> lock(endpoint_mutex_);
            endpoint_ = *resolved_endpoints.begin();
            socket_.connect(endpoint_);
        }
        
        LOG(INFO) << "Connected to " << endpoint;
        
        // For UDP, send a handshake packet to trigger the server's listen callback
        uint8_t handshake = 0;
        if (!write(&handshake, sizeof(handshake))) {
            LOG(ERROR) << "Failed to send handshake packet";
            return false;
        }
        LOG(INFO) << "Sent handshake packet to server";
        
        return true;
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "Connect error: " << e.what();
        return false;
    }
}

Context::RemoteEndpoint Context::UDP::remoteAddress() const {
    try {
        std::lock_guard<std::mutex> lock(endpoint_mutex_);
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
