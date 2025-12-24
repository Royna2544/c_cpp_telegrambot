#include <absl/log/log.h>
#include <fmt/chrono.h>
#include <fmt/format.h>

#include <GitBuildInfo.hpp>
#include <boost/asio/use_future.hpp>
#include <condition_variable>
#include <future>
#include <string_view>

#include "ApiDef.hpp"
#include "../SocketContext.hpp"

template <>
struct fmt::formatter<boost::asio::ip::tcp> : formatter<string_view> {
    // parse is inherited from formatter<string_view>.
    auto format(boost::asio::ip::tcp c, format_context& ctx) const
        -> format_context::iterator {
        std::string_view name;
        if (c == boost::asio::ip::tcp::v4()) {
            name = "IPv4";
        } else if (c == boost::asio::ip::tcp::v6()) {
            name = "IPv6";
        } else {
            name = "Unknown";
        }
        return formatter<string_view>::format(name, ctx);
    }
};

namespace TgBotSocket {

auto getendpoint(const boost::asio::ip::tcp type, const uint_least16_t port) {
    using namespace boost::asio::ip;
    if constexpr (buildinfo::isReleaseBuild()) {
        return tcp::endpoint(type, port);
    } else {
        return tcp::endpoint(make_address("0.0.0.0"), port);
    }
}

Context::TCP::TCP(const boost::asio::ip::tcp type, const uint_least16_t port)
    : socket_(io_context),
      acceptor_(io_context),
      endpoint_(getendpoint(type, port)) {
    LOG(INFO) << fmt::format("TCP::TCP: Protocol {} listening on port {}", type,
                             port);

    work_guard_.emplace(boost::asio::make_work_guard(io_context));
    _ioThread = std::thread([this]() {
        io_context.run();
    });
}

Context::TCP::~TCP() {
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

template <class Future, class Duration>
bool wait_until_ready(Future& fut, const Duration& d,
                      boost::asio::ip::tcp::socket& sock,
                      const std::string& tag) {
    if (fut.wait_until(std::chrono::steady_clock::now() + d) !=
        std::future_status::ready) {
        LOG(ERROR) << tag << ": timeout";
        boost::system::error_code ec;
        sock.cancel(ec);
        return false;
    }
    return true;
}

bool Context::TCP::writeNonBlocking(const uint8_t* data, size_t length) const {
    try {
        auto fut =
            boost::asio::async_write(socket_, boost::asio::buffer(data, length),
                                     boost::asio::use_future);
        if (!wait_until_ready(fut, options.io_timeout.get(), socket_,
                              "TCP::write"))
            return false;
        const std::size_t bytes = fut.get();
        return bytes == length;
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "TCP::write: " << e.what();
        return false;
    }
}

bool Context::TCP::writeBlocking(const uint8_t* data, size_t length) const {
    try {
        boost::asio::write(socket_, boost::asio::buffer(data, length));
        return true;
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "TCP::write: " << e.what();
        return false;
    }
}

bool Context::TCP::write(const uint8_t* data, size_t length) const {
    return static_cast<bool>(options.io_timeout)
               ? writeNonBlocking(data, length)
               : writeBlocking(data, length);
}

std::optional<SharedMalloc> Context::TCP::readNonBlocking(
    Packet::Header::length_type length) const {
    SharedMalloc buf(length);
    auto fut =
        boost::asio::async_read(socket_, boost::asio::buffer(buf.get(), length),
                                boost::asio::use_future);
    if (!wait_until_ready(fut, options.io_timeout.get(), socket_, "TCP::read"))
        return std::nullopt;
    std::size_t bytes = 0;
    try {
        bytes = fut.get();
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "TCP::read: " << e.what();
        return std::nullopt;
    }
    buf.resize(bytes);
    return buf;
}

std::optional<SharedMalloc> Context::TCP::readBlocking(
    Packet::Header::length_type length) const {
    SharedMalloc buf(length);
    try {
        boost::asio::read(socket_, boost::asio::buffer(buf.get(), length));
        return buf;
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "TCP::read: " << e.what();
        return std::nullopt;
    }
}

std::optional<SharedMalloc> Context::TCP::read(
    Packet::Header::length_type length) const {
    return static_cast<bool>(options.io_timeout) ? readNonBlocking(length)
                                                 : readBlocking(length);
}
bool Context::TCP::close() const {
    boost::system::error_code ec;
    using namespace boost::asio::ip;
    socket_.shutdown(tcp::socket::shutdown_both, ec);  // NOLINT
    if (ec) {
        LOG(WARNING) << "Cannot shutdown socket: " << ec.message();
    }
    socket_.close(ec);  // NOLINT
    if (ec) {
        LOG(WARNING) << "Cannot close socket: " << ec.message();
    }
    return !ec;
}

bool Context::TCP::listen(listener_callback_t listener, bool block) {
    try {
        if (!is_listening_) {
            acceptor_.open(endpoint_.protocol());
            acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
            acceptor_.bind(endpoint_);
            acceptor_.listen();
            is_listening_ = true;
        }
        acceptor_.async_accept(
            socket_, [this, listener, block](boost::system::error_code ec) {
                if (ec) {
                    LOG(ERROR) << "Accept error: " << ec.message();
                    return;
                }
                LOG(INFO) << "Handling new connection: Client address: "
                          << remoteAddress();
                listener(*this);
                if (!io_context.stopped()) {
                    // After handling the connection, create a new socket for
                    // the next client
                    socket_ = boost::asio::ip::tcp::socket(io_context);
                    listen(listener, block);
                }
            });
        if (block) {
            io_context.run();
        }
        return true;
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "TCP::listen: " << e.what();
        return false;
    }
}

bool Context::TCP::connect(const RemoteEndpoint& endpoint) {
    // Resolve the address and port
    boost::asio::ip::tcp::resolver resolver(io_context);
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
                  const boost::asio::ip::tcp::endpoint& /*ep*/) {
            {
                std::unique_lock<std::mutex> lock(m);
                if (ec) {
                    LOG(ERROR) << "Connect error: " << ec.message();
                } else {
                    connected = true;
                }
            }
            cv.notify_all();
        });

    // Wait for the connection to complete
    LOG(INFO) << fmt::format("Waiting for connection to complete for {}",
                             options.connect_timeout.get());
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

Context::RemoteEndpoint Context::TCP::remoteAddress() const {
    try {
        auto remote_ep = socket_.remote_endpoint();
        return {.address = remote_ep.address().to_string(),
                .port = remote_ep.port()};
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "TCP::remoteAddress: " << e.what();
        return {};
    }
}

bool Context::TCP::abortConnections() {
    boost::system::error_code ec;
    acceptor_.close(ec);  // NOLINT
    if (ec) {
        LOG(WARNING) << "Cannot close acceptor: " << ec.message();
    }
    close();
    io_context.stop();
    return !ec;
}

Context::TCP::operator bool() const { return socket_.is_open(); }

}  // namespace TgBotSocket
