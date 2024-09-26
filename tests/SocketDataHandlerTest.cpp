#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <SharedMalloc.hpp>
#include <cstdint>
#include <impl/bot/TgBotSocketInterface.hpp>
#include <memory>
#include <utility>

#include "SocketBase.hpp"
#include "TgBotSocket_Export.hpp"
#include "commands/CommandModulesTest.hpp"

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

class SocketDataHandlerTest : public ::testing::Test {
    static constexpr int kSocket = 1000;

   public:
    SocketDataHandlerTest()
        : _mockImpl(std::make_shared<SocketInterfaceImplMock>()),
          _mockApi(std::make_shared<MockTgBotApi>()),
          mockInterface(_mockImpl, _mockApi),
          fakeConn(kSocket, nullptr) {}

    std::shared_ptr<SocketInterfaceImplMock> _mockImpl;
    std::shared_ptr<MockTgBotApi> _mockApi;
    SocketInterfaceTgBot mockInterface;
    // Dummy, not under a real connection
    SocketConnContext fakeConn;

    /**
     * @brief Send a command packet and verify the header and assigns data to
     * the out parameter.
     *
     * @param pkt The command packet to send.
     * @param out [out] The data assigned to the packet.
     * @tparam T The type of the data to be consumed.
     */
    template <typename DataT, TgBotSocket::Command retCmd>
    void sendAndVerifyHeader(TgBotSocket::Packet pkt, DataT* out) {
        SharedMalloc packetData;
        TgBotSocket::PacketHeader recv_header;

        EXPECT_CALL(*_mockImpl, writeToSocket(fakeConn, _))
            .WillOnce(DoAll(SaveArg<1>(&packetData), Return(true)));
        mockInterface.handle_CommandPacket(fakeConn, std::move(pkt));

        // Expect valid packet
        EXPECT_TRUE(packetData.get());
        // Checking packet header
        EXPECT_NO_FATAL_FAILURE(packetData.assignTo(recv_header));
        EXPECT_EQ(recv_header.cmd, retCmd);
        EXPECT_EQ(recv_header.data_size, sizeof(DataT));
        // Checking packet data
        EXPECT_NO_FATAL_FAILURE(packetData.assignTo(
            out, sizeof(DataT), TgBotSocket::Packet::hdr_sz));
    }

    static void isGenericAck_OK(const TgBotSocket::callback::GenericAck& ack) {
        EXPECT_STREQ(ack.error_msg.data(), "OK");
        EXPECT_EQ(ack.result, TgBotSocket::callback::AckType::SUCCESS);
    }

    template <TgBotSocket::callback::AckType type>
    static void isGenericAck_Error(
        const TgBotSocket::callback::GenericAck& ack) {
        EXPECT_NE(ack.error_msg.data(), "OK");
        EXPECT_EQ(ack.result, type);
    }

    /**
     * @brief Verify and clear all expectations on the mock objects.
     */
    void verifyAndClear() {
        testing::Mock::VerifyAndClearExpectations(_mockImpl.get());
        testing::Mock::VerifyAndClearExpectations(_mockApi.get());
    }
};

TEST_F(SocketDataHandlerTest, TestCmdGetUptime) {
    // data Unused for GetUptime
    TgBotSocket::Packet pkt(TgBotSocket::Command::CMD_GET_UPTIME, 0);
    TgBotSocket::callback::GetUptimeCallback callbackData{};

    sendAndVerifyHeader<TgBotSocket::callback::GetUptimeCallback,
                        TgBotSocket::Command::CMD_GET_UPTIME_CALLBACK>(
        pkt, &callbackData);
    EXPECT_STREQ(callbackData.uptime.data(), "Uptime: 0h 0m 0s");
    // Done
    verifyAndClear();
}

TEST_F(SocketDataHandlerTest, TestCmdWriteMsgToChatId) {
    constexpr ChatId testChatId = 101848141293;
    TgBotSocket::data::WriteMsgToChatId data{
        .chat = testChatId,
        .message = {"Hello, World!"},
    };
    TgBotSocket::Packet pkt(TgBotSocket::Command::CMD_WRITE_MSG_TO_CHAT_ID,
                            data);
    EXPECT_CALL(*_mockApi, sendMessage_impl(testChatId, data.message.data(),
                                            IsNull(), IsNull(), ""));
    TgBotSocket::callback::GenericAck callbackData{};
    sendAndVerifyHeader<TgBotSocket::callback::GenericAck,
                        TgBotSocket::Command::CMD_GENERIC_ACK>(pkt,
                                                               &callbackData);
    isGenericAck_OK(callbackData);
    // Done
    verifyAndClear();
}

TEST_F(SocketDataHandlerTest, TestCmdWriteMsgToChatIdTgBotApiEx) {
    constexpr ChatId testChatId = 1848141293;
    TgBotSocket::data::WriteMsgToChatId data{
        .chat = testChatId,
        .message = {"Hello, World!"},
    };
    TgBotSocket::Packet pkt(TgBotSocket::Command::CMD_WRITE_MSG_TO_CHAT_ID,
                            data);
    EXPECT_CALL(*_mockApi, sendMessage_impl(testChatId, data.message.data(),
                                            IsNull(), IsNull(), ""))
        .WillOnce(testing::Throw(TgBot::TgException(
            "AAAAA", TgBot::TgException::ErrorCode::Forbidden)));

    TgBotSocket::callback::GenericAck callbackData{};
    sendAndVerifyHeader<TgBotSocket::callback::GenericAck,
                        TgBotSocket::Command::CMD_GENERIC_ACK>(pkt,
                                                               &callbackData);
    isGenericAck_Error<TgBotSocket::callback::AckType::ERROR_TGAPI_EXCEPTION>(
        callbackData);
    // Done
    verifyAndClear();
}

TEST_F(SocketDataHandlerTest, TestCmdWriteMsgToChatIdINVALID) {
    constexpr ChatId testChatId = 101848141293;
    struct {
        TgBotSocket::data::WriteMsgToChatId realdata{
            .chat = testChatId,
            .message = {"Hello, World!"},
        };
        std::uintmax_t FAKEVALUE = 0;
    } data;

    // For this test to be valid...
    static_assert(sizeof(data) > sizeof(data.realdata));
    //...the size of the test data must be larger than the size of the real data.

    TgBotSocket::Packet pkt(TgBotSocket::Command::CMD_WRITE_MSG_TO_CHAT_ID,
                            data);
    TgBotSocket::callback::GenericAck callbackData{};
    sendAndVerifyHeader<TgBotSocket::callback::GenericAck,
                        TgBotSocket::Command::CMD_GENERIC_ACK>(pkt,
                                                               &callbackData);
    isGenericAck_Error<TgBotSocket::callback::AckType::ERROR_COMMAND_IGNORED>(callbackData);
    // Done
    verifyAndClear();
}