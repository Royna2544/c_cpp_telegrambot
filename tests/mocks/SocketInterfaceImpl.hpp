#pragma once

#include <gmock/gmock.h>

#include <socket/SocketContext.hpp>
#include <trivial_helpers/fruit_inject.hpp>

class MockContext : public TgBotSocket::Context {
   public:
    APPLE_INJECT(MockContext()) = default;
    MOCK_METHOD(bool, write, (const uint8_t* data, const size_t length),
                (const, override));
    MOCK_METHOD(std::optional<SharedMalloc>, read,
                (TgBotSocket::Packet::Header::length_type length),
                (const, override));
    MOCK_METHOD(bool, close, (), (const, override));
    MOCK_METHOD(bool, listen, (listener_callback_t listener, bool block),
                (override));
    MOCK_METHOD(bool, connect, (const RemoteEndpoint& endpoint), (override));
    MOCK_METHOD(RemoteEndpoint, remoteAddress, (), (const, override));
    MOCK_METHOD(bool, abortConnections, (), (override));
    MOCK_METHOD(bool, ok, (), (const));

    operator bool() const override { return ok(); }

    // Optionally, you can mock any additional methods or members if needed.
};
