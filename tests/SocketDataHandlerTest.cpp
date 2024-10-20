#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <tgbot/TgException.h>

#include <SharedMalloc.hpp>
#include <cstdint>
#include <filesystem>
#include <impl/bot/TgBotSocketFileHelperNew.hpp>
#include <impl/bot/TgBotSocketInterface.hpp>
#include <memory>
#include <string_view>
#include <utility>

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

class VFSOperationsMock : public VFSOperations {
   public:
    MOCK_METHOD(bool, writeFile,
                (const std::filesystem::path& filename,
                 const uint8_t* startAddr, size_t size),
                (override));

    MOCK_METHOD(std::optional<SharedMalloc>, readFile,
                (const std::filesystem::path& filename), (override));

    MOCK_METHOD(bool, exists, (const std::filesystem::path& path), (override));

    MOCK_METHOD(void, SHA256, (const SharedMalloc& memory, HashContainer& data),
                (override));
};

class SocketDataHandlerTest : public ::testing::Test {
    static constexpr int kSocket = 1000;

   public:
    SocketDataHandlerTest()
        : _mockImpl(std::make_shared<SocketInterfaceImplMock>()),
          _mockApi(std::make_shared<MockTgBotApi>()),
          _mockVFS(std::make_unique<VFSOperationsMock>()),
          mockInterface(_mockImpl, _mockApi.get(),
                        std::make_shared<SocketFile2DataHelper>(_mockVFS)),
          fakeConn(kSocket, nullptr) {}

    std::shared_ptr<SocketInterfaceImplMock> _mockImpl;
    std::shared_ptr<MockTgBotApi> _mockApi;
    std::shared_ptr<VFSOperationsMock> _mockVFS;
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
        mockInterface.handlePacket(fakeConn, std::move(pkt));

        // Expect valid packet
        EXPECT_TRUE(packetData.get());
        // Checking packet header
        EXPECT_NO_FATAL_FAILURE(packetData.assignTo(recv_header));
        EXPECT_EQ(recv_header.cmd, retCmd);
        ASSERT_EQ(recv_header.data_size, sizeof(DataT));
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
        testing::Mock::VerifyAndClearExpectations(_mockVFS.get());
    }

    static SharedMalloc createFileMem() {
        constexpr std::string_view fileContent =
            "Alex is very dumb, go sleep kid.";
        SharedMalloc fileMem(fileContent.size());
        fileMem.assignFrom(fileContent.data(), fileContent.size());
        return fileMem;
    }

    using FSP = std::filesystem::path;
};

TEST_F(SocketDataHandlerTest, TestCmdGetUptime) {
    // data Unused for GetUptime
    TgBotSocket::Packet pkt(TgBotSocket::Command::CMD_GET_UPTIME, 0);
    TgBotSocket::callback::GetUptimeCallback callbackData{};

    sendAndVerifyHeader<TgBotSocket::callback::GetUptimeCallback,
                        TgBotSocket::Command::CMD_GET_UPTIME_CALLBACK>(
        pkt, &callbackData);
    EXPECT_STREQ(callbackData.uptime.data(), "Uptime: 00:00:00.00");
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
        // Fake padding to increase data size
        std::uintmax_t FAKEVALUE = 0;
    } data;

    // For this test to be valid...
    static_assert(sizeof(data) > sizeof(data.realdata));
    //...the size of the test data must be larger than the size of the real
    // data.

    TgBotSocket::Packet pkt(TgBotSocket::Command::CMD_WRITE_MSG_TO_CHAT_ID,
                            data);
    TgBotSocket::callback::GenericAck callbackData{};
    sendAndVerifyHeader<TgBotSocket::callback::GenericAck,
                        TgBotSocket::Command::CMD_GENERIC_ACK>(pkt,
                                                               &callbackData);
    isGenericAck_Error<TgBotSocket::callback::AckType::ERROR_COMMAND_IGNORED>(
        callbackData);
    // Done
    verifyAndClear();
}

TEST_F(SocketDataHandlerTest, TestCmdUploadFileDryDoesntExist) {
    auto data = UploadFileDry{.destfilepath = {"test"},
                              .srcfilepath = {"testsrc"},
                              .sha256_hash = {"asdqwdsadsad"},
                              .options = {
                                  .overwrite = true,
                                  .hash_ignore = true,
                                  .dry_run = true,
                              }};

    // Set expectations
    TgBotSocket::Packet pkt(TgBotSocket::Command::CMD_UPLOAD_FILE_DRY, data);
    EXPECT_CALL(*_mockVFS, exists(FSP(data.destfilepath.data())))
        .WillOnce(Return(false));

    // Verify result
    TgBotSocket::callback::UploadFileDryCallback callback;
    sendAndVerifyHeader<TgBotSocket::callback::UploadFileDryCallback,
                        TgBotSocket::Command::CMD_UPLOAD_FILE_DRY_CALLBACK>(
        pkt, &callback);
    EXPECT_NE(callback.result,
              TgBotSocket::callback::AckType::ERROR_COMMAND_IGNORED);
    EXPECT_EQ(callback.requestdata, data);
}

TEST_F(SocketDataHandlerTest, TestCmdUploadFileDryExistsHashDoesntMatch) {
    auto data = UploadFileDry{.destfilepath = {"test"},
                              .srcfilepath = {"testsrc"},
                              .sha256_hash = {"asdqwdsadsad"},
                              .options = {
                                  .overwrite = true,
                                  .hash_ignore = false,
                                  .dry_run = true,
                              }};
    // Prepare file contents
    const auto fileMem = createFileMem();

    // Set expectations
    TgBotSocket::Packet pkt(TgBotSocket::Command::CMD_UPLOAD_FILE_DRY, data);
    EXPECT_CALL(*_mockVFS, exists(FSP(data.destfilepath.data())))
        .WillOnce(Return(true));
    EXPECT_CALL(*_mockVFS, readFile(FSP(data.destfilepath.data())))
        .WillOnce(Return(fileMem));
    EXPECT_CALL(*_mockVFS, SHA256(fileMem, _))
        .WillOnce(testing::SetArgReferee<1>(HashContainer{data.sha256_hash}));

    // Verify result
    TgBotSocket::callback::UploadFileDryCallback callback;
    sendAndVerifyHeader<TgBotSocket::callback::UploadFileDryCallback,
                        TgBotSocket::Command::CMD_UPLOAD_FILE_DRY_CALLBACK>(
        pkt, &callback);
    EXPECT_NE(callback.result, TgBotSocket::callback::AckType::SUCCESS);
    EXPECT_EQ(callback.requestdata, data);
}

TEST_F(SocketDataHandlerTest, TestCmdUploadFileDryExistsOptSaidNo) {
    auto data = UploadFileDry{.destfilepath = {"test"},
                              .srcfilepath = {"testsrc"},
                              .sha256_hash = {"asdqwdsadsad"},
                              .options = {
                                  .overwrite = false,
                                  .hash_ignore = false,
                                  .dry_run = true,
                              }};
    // Prepare file contents
    const auto fileMem = createFileMem();

    // Set expectations
    TgBotSocket::Packet pkt(TgBotSocket::Command::CMD_UPLOAD_FILE_DRY, data);
    EXPECT_CALL(*_mockVFS,
                exists(std::filesystem::path(data.destfilepath.data())))
        .WillOnce(Return(true));

    // Verify result
    TgBotSocket::callback::UploadFileDryCallback callback;
    sendAndVerifyHeader<TgBotSocket::callback::UploadFileDryCallback,
                        TgBotSocket::Command::CMD_UPLOAD_FILE_DRY_CALLBACK>(
        pkt, &callback);
    EXPECT_EQ(callback.result,
              TgBotSocket::callback::AckType::ERROR_COMMAND_IGNORED);
    EXPECT_EQ(callback.requestdata, data);
}

TEST_F(SocketDataHandlerTest, TestCmdUploadFileOK) {
    // Prepare file contents
    const auto filemem = createFileMem();
    SharedMalloc mem(sizeof(UploadFile) + filemem->size());
    auto* uploadfile = static_cast<UploadFile*>(mem.get());
    uploadfile->srcfilepath = {"sourcefile"};
    uploadfile->destfilepath = {"destinationfile"};
    uploadfile->sha256_hash = {"asdqwdsadsad"};
    uploadfile->options.hash_ignore = true;
    uploadfile->options.overwrite = true;
    uploadfile->options.dry_run = false;
    mem.assignTo(filemem.get(), filemem->size(), sizeof(UploadFile));

    // Set expectations
    TgBotSocket::Packet pkt(TgBotSocket::Command::CMD_UPLOAD_FILE, mem.get(),
                            mem->size());
    EXPECT_CALL(*_mockVFS, writeFile(FSP(uploadfile->destfilepath.data()), _,
                                     filemem->size()))
        .WillOnce(Return(true));

    // Verify result
    TgBotSocket::callback::GenericAck callback;
    sendAndVerifyHeader<TgBotSocket::callback::GenericAck,
                        TgBotSocket::Command::CMD_GENERIC_ACK>(pkt, &callback);
    isGenericAck_OK(callback);
}