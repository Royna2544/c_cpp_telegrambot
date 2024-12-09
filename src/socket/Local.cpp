#include <absl/log/log.h>

#include <filesystem>
#include <system_error>

#include "SocketContext.hpp"

namespace TgBotSocket {

Context::Local::Local(const std::filesystem::path& path)
    : socket_(io_context), acceptor_(io_context), endpoint_(path.string()) {
    LOG(INFO) << "Local::Local: Path=" << path;
    std::error_code ec;
    if (std::filesystem::exists(path, ec)) {
        std::filesystem::remove(path, ec);
        LOG(INFO) << "Removed stale file.";
    }
    if (ec) {
        LOG(ERROR) << "Cannot remove stale file: " << ec.message();
    }
}

Context::Local::Local() : socket_(io_context), acceptor_(io_context) {}

bool Context::Local::write(const SharedMalloc& data) const {
    try {
        boost::asio::write(socket_,
                           boost::asio::buffer(data.get(), data.size()));
        return true;
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "Local::write: " << e.what();
        return false;
    }
}

std::optional<SharedMalloc> Context::Local::read(
    Packet::Header::length_type length) const {
    try {
        SharedMalloc buffer(length);
        size_t bytes_read = boost::asio::read(
            socket_, boost::asio::buffer(buffer.get(), length));
        buffer.resize(bytes_read);
        return buffer;
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "Local::read: " << e.what();
        return std::nullopt;
    }
}

bool Context::Local::close() const {
    boost::system::error_code ec;
    using namespace boost::asio::local;
    socket_.shutdown(stream_protocol::socket::shutdown_both, ec);  // NOLINT
    if (ec) {
        LOG(WARNING) << "Cannot shutdown socket: " << ec.message();
    }
    socket_.close(ec);  // NOLINT
    if (ec) {
        LOG(WARNING) << "Cannot close socket: " << ec.message();
    }
    return !ec;
}

bool Context::Local::timeout(std::chrono::seconds time) {
    // Store the timeout duration for potential use in operations
    timeout_duration_ = time;
    return true;
}

bool Context::Local::listen(listener_callback_t listener) {
    try {
        if (!is_listening_) {
            acceptor_.open();
            boost::system::error_code ec;
            acceptor_.bind(endpoint_, ec);  // NOLINT
            if (ec) {
                LOG(ERROR) << "Cannot bind to endpoint: " << ec.message();
                return false;
            }
            acceptor_.listen();
            is_listening_ = true;
        }
        acceptor_.async_accept(
            socket_, [this, listener](boost::system::error_code ec) {
                if (ec) {
                    LOG(ERROR) << "Accept error: " << ec.message();
                    return;
                }
                LOG(INFO) << "Accepted local connection from: "
                          << remoteAddress().address;
                listener(*this);
                // After handling the connection, create a new socket for
                // the next client
                if (!io_context.stopped()) {
                    socket_ =
                        boost::asio::local::stream_protocol::socket(io_context);
                    listen(listener);
                }
            });
        io_context.run();
        return true;
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "Local::listen: " << e.what();
        return false;
    }
}

bool Context::Local::connect(const RemoteEndpoint& endpoint) {
    try {
        // Attempt to connect to the resolved endpoints
        socket_.connect(
            boost::asio::local::stream_protocol::endpoint(endpoint.address));

        // Connection successful
        LOG(INFO) << "Connected to " << endpoint;
        return true;
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "Local::connect: " << e.what();
        return false;
    }
}

Context::RemoteEndpoint Context::Local::remoteAddress() const {
    // For local sockets, the remote endpoint may not be available
    // We can return the path of the socket
    return {.address=endpoint_.path(), .port=0};
}

bool Context::Local::abortConnections() {
    boost::system::error_code ec;
    acceptor_.close(ec);  // NOLINT
    if (ec) {
        LOG(WARNING) << "Cannot close acceptor: " << ec.message();
    }
    if (operator bool()) {
        close();
    }
    io_context.stop();
    return !ec;
}

Context::Local::operator bool() const { return socket_.is_open(); }

}  // namespace TgBotSocket
