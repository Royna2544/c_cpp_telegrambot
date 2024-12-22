#include <absl/log/log.h>
#include <fmt/chrono.h>
#include <fmt/format.h>

#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/use_future.hpp>
#include <filesystem>
#include <system_error>

#include "SocketContext.hpp"

namespace TgBotSocket {

#ifndef BOOST_ASIO_HAS_LOCAL_SOCKETS
#error "Local sockets are not supported on this platform."
#endif

Context::Local::Local(const std::filesystem::path& path)
    : socket_(io_context), acceptor_(io_context), endpoint_(path.string()) {
    LOG(INFO) << "Local::Local: Path=" << path;
    std::error_code ec;

    if (std::filesystem::remove(path, ec)) {
        LOG(INFO) << "Removed stale file.";
    } else if (ec && ec != std::make_error_code(
                               std::errc::no_such_file_or_directory)) {
        LOG(ERROR) << "Cannot remove stale file: " << ec.message();
    }

    _ioThread = std::thread([this]() {
        auto work = boost::asio::make_work_guard(io_context);
        io_context.run();
    });
}

Context::Local::Local() : socket_(io_context), acceptor_(io_context) {
    LOG(INFO) << "Local::Local: Path=UNSPECIFIED";
    _ioThread = std::thread([this]() {
        auto work = boost::asio::make_work_guard(io_context);
        io_context.run();
    });
}

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

Context::Local::~Local() {
    if (operator bool()) {
        close();
    }
    io_context.stop();
    if (_ioThread.joinable()) {
        _ioThread.join();
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
    // Create a condition variable to wait for the connection to complete
    std::condition_variable cv;
    std::mutex m;
    std::unique_lock<std::mutex> lk(m);
    bool connected = false;

    // Attempt to connect to the resolved endpoints
    socket_.async_connect(
        boost::asio::local::stream_protocol::endpoint(endpoint.address),
        [&, this](boost::system::error_code ec) {
            if (ec) {
                LOG(ERROR) << "Connect error: " << ec.message();
            } else {
                DLOG(INFO) << "Connected to " << endpoint.address;
                connected = true;
            }
            cv.notify_all();
        });

    if (timeout_duration_.count() > 0) {
        // Wait for the connection to complete
        LOG(INFO) << fmt::format("Waiting for connection to complete for {}",
                                 timeout_duration_);
        cv.wait_for(lk, timeout_duration_,
                    [&] { return connected || io_context.stopped(); });
    } else {
        // Wait for the connection to complete
        cv.wait(lk, [&] { return connected || io_context.stopped(); });
    }

    // Port is always 0 for local domain sockets, so we don't need to print it.
    if (!connected) {
        LOG(ERROR) << "Failed to connect to " << endpoint.address;
        return false;
    }

    // Connection successful
    LOG(INFO) << "Connected to " << endpoint.address;
    return true;
}

Context::RemoteEndpoint Context::Local::remoteAddress() const {
    // For local sockets, the remote endpoint may not be available
    // We can return the path of the socket
    return {.address = endpoint_.path(), .port = 0};
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
