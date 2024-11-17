#pragma once

#include <SocketBase.hpp>
#include <gmock/gmock.h>
#include <trivial_helpers/fruit_inject.hpp>

class SocketInterfaceImplMock : public SocketInterfaceBase {
   public:
    APPLE_INJECT(SocketInterfaceImplMock()) = default;

    MOCK_METHOD(bool, isValidSocketHandle, (socket_handle_t handle),
                (override));
    MOCK_METHOD(bool, writeToSocket,
                (SocketConnContext context, SharedMalloc data), (override));
    MOCK_METHOD(bool, forceStopListening, (), (override));
    MOCK_METHOD(void, startListening,
                (socket_handle_t handle, const listener_callback_t onNewData),
                (override));
    MOCK_METHOD(bool, closeSocketHandle, (socket_handle_t & handle),
                (override));
    MOCK_METHOD(bool, setSocketOptTimeout,
                (socket_handle_t handle, int timeout), (override));
    MOCK_METHOD(std::optional<SharedMalloc>, readFromSocket,
                (SocketConnContext context, buffer_len_t length), (override));
    MOCK_METHOD(std::optional<SocketConnContext>, createClientSocket, (),
                (override));
    MOCK_METHOD(std::optional<socket_handle_t>, createServerSocket, (),
                (override));
    MOCK_METHOD(void, printRemoteAddress, (socket_handle_t handle), (override));
};
