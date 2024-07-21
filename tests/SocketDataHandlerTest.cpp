#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <SharedMalloc.hpp>
#include <impl/bot/TgBotSocketInterface.hpp>
#include <memory>

#include "SocketBase.hpp"
#include "TgBotSocket_Export.hpp"
#include "gmock/gmock.h"

using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;

class SocketInterfaceImplMock : public SocketInterfaceBase {
   public:
    MOCK_METHOD(bool, isValidSocketHandle, (socket_handle_t handle),
                (override));
    MOCK_METHOD(bool, writeToSocket,
                (SocketConnContext context, SharedMalloc data), (override));
    MOCK_METHOD(void, forceStopListening, (), (override));
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

auto inst = std::make_shared<SocketInterfaceImplMock>();
class SocketDataHandlerTest : public ::testing::Test {
    static constexpr int kSocket = 1000;

   public:
    SocketDataHandlerTest()
        : mockInterface(inst), fakeConn(kSocket, NULL), _mockImpl(inst) {}

    SocketInterfaceTgBot mockInterface;
    // Dummy, not under a real connection
    SocketConnContext fakeConn;
    std::shared_ptr<SocketInterfaceImplMock> _mockImpl;
};

TEST_F(SocketDataHandlerTest, TestCmdGetUptime) {
    // data Unused for GetUptime
    TgBotSocket::Packet pkt(TgBotSocket::Command::CMD_GET_UPTIME, 0);
    TgBotSocket::PacketHeader recv_header;
    SharedMalloc packetData;
    using cbt = TgBotSocket::callback::GetUptimeCallback;
    cbt callbackData{};

    EXPECT_CALL(*_mockImpl, writeToSocket(fakeConn, _))
        .WillOnce(DoAll(SaveArg<1>(&packetData), Return(true)));
    mockInterface.handle_CommandPacket(fakeConn, pkt);
    EXPECT_TRUE(packetData.get());
    EXPECT_NO_FATAL_FAILURE(packetData.assignTo(recv_header));
    EXPECT_EQ(recv_header.cmd, TgBotSocket::Command::CMD_GET_UPTIME_CALLBACK);
    EXPECT_EQ(recv_header.data_size, sizeof(cbt));
    packetData.assignTo(&callbackData, sizeof(cbt), TgBotSocket::Packet::hdr_sz);
    EXPECT_STREQ(callbackData.uptime.data(), "Uptime: 0h 0m 0s");
    testing::Mock::VerifyAndClearExpectations(&_mockImpl);
}