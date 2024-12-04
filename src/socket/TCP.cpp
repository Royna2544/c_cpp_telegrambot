#include <absl/log/log.h>
#include <fmt/format.h>

#include <string_view>

#include "SocketContext.hpp"
#include "TgBotSocket_Export.hpp"

template <>
struct fmt::formatter<boost::asio::ip::tcp> : formatter<string_view> {
    // parse is inherited from formatter<string_view>.
    auto format(boost::asio::ip::tcp c,
                format_context& ctx) const -> format_context::iterator {
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

Context::TCP::TCP(const boost::asio::ip::tcp type, const int port)
    : socket_(io_context),
      acceptor_(io_context),
      endpoint_(boost::asio::ip::tcp::endpoint(type, port)) {
    LOG(INFO) << fmt::format("TCP::TCP: Protocol {} listening on port {}", type,
                             port);
}

bool Context::TCP::write(const SharedMalloc& data) const {
    try {
        std::size_t total_size = data.size();
        std::size_t bytes_written = 0;
        const char* data_ptr = (char*)data.get();

        DLOG(INFO) << "TCP::write:" << data.size() << " bytes requested";
        while (bytes_written < total_size) {
            // Calculate the size of the next chunk
            std::size_t write_size = std::min<Packet::Header::length_type>(
                chunk_size, total_size - bytes_written);

            // Write the current chunk
            std::size_t written = boost::asio::write(
                socket_,
                boost::asio::buffer(data_ptr + bytes_written, write_size));

            bytes_written += written;

            // Log or update progress
            LOG_EVERY_N_SEC(INFO, 5) << "TCP::write: Sent " << bytes_written
                                     << " / " << total_size << " bytes.";

            // delay 10ms
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        DLOG(INFO) << "TCP::write: Finished sending " << total_size
                   << " bytes.";
        return true;

    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "TCP::write: " << e.what();
        return false;
    }
}

std::optional<SharedMalloc> Context::TCP::read(
    Packet::Header::length_type length) const {
    try {
        SharedMalloc buffer(length);
        char* buffer_ptr = (char*)buffer.get();
        size_t bytes_read = 0;

        DLOG(INFO) << "TCP::read:" << length << " bytes requested";
        while (bytes_read < length) {
            // Calculate the size of the next chunk to read
            std::size_t read_size = std::min<Packet::Header::length_type>(
                chunk_size, length - bytes_read);

            // Read the current chunk
            size_t chunk_bytes = boost::asio::read(
                socket_,
                boost::asio::buffer(buffer_ptr + bytes_read, read_size));

            bytes_read += chunk_bytes;

            // Log or update progress
            LOG_EVERY_N_SEC(INFO, 5) << "TCP::read: Received " << bytes_read
                                     << " / " << length << " bytes.";

            if (io_context.stopped()) {
                break;
            }
        }

        // Adjust the buffer size to the actual number of bytes read (if needed)
        buffer.resize(bytes_read);

        DLOG(INFO) << "TCP::read: Finished receiving " << bytes_read
                   << " bytes.";
        return buffer;

    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "TCP::read: " << e.what();
        return std::nullopt;
    }
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

bool Context::TCP::timeout(std::chrono::seconds time) {
    // Store the timeout duration for use in read/write operations
    timeout_duration_ = time;
    return true;
}

bool Context::TCP::listen(listener_callback_t listener) {
    try {
        if (!is_listening_) {
            acceptor_.open(endpoint_.protocol());
            acceptor_.set_option(boost::asio::socket_base::reuse_address(true));
            acceptor_.bind(endpoint_);
            acceptor_.listen();
            is_listening_ = true;
        }
        acceptor_.async_accept(
            socket_, [this, listener](boost::system::error_code ec) {
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
                    listen(listener);
                }
            });
        io_context.run();
        return true;
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "TCP::listen: " << e.what();
        return false;
    }
}

bool Context::TCP::connect(const RemoteEndpoint& endpoint) {
    try {
        // Resolve the address and port
        boost::asio::ip::tcp::resolver resolver(io_context);
        auto endpoints =
            resolver.resolve(endpoint.address, std::to_string(endpoint.port));

        // Attempt to connect to the resolved endpoints
        boost::asio::connect(socket_, endpoints);

        // Connection successful
        LOG(INFO) << "Connected to " << endpoint;
        return true;
    } catch (const boost::system::system_error& e) {
        LOG(ERROR) << "TCP::connect: " << e.what();
        return false;
    }
}

Context::RemoteEndpoint Context::TCP::remoteAddress() const {
    try {
        auto remote_ep = socket_.remote_endpoint();
        return {remote_ep.address().to_string(), remote_ep.port()};
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