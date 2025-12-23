#include "CommandModulesTest.hpp"

using testing::_;
using testing::HasSubstr;
using testing::Return;

class UpDownCommandTest : public CommandTestBase {
   public:
    UpDownCommandTest() : CommandTestBase("up") {
        ON_CALL(strings, get(Strings::FAILED_TO_READ_FILE))
            .WillByDefault(Return("Failed to read file"));
        ON_CALL(strings, get(Strings::FAILED_TO_DOWNLOAD_FILE))
            .WillByDefault(Return("Failed to download"));
        ON_CALL(strings, get(Strings::OPERATION_SUCCESSFUL))
            .WillByDefault(Return("Success"));
    }
    ~UpDownCommandTest() override = default;
};

TEST_F(UpDownCommandTest, UploadNonExistentFile) {
    setCommandExtArgs({"/nonexistent/file.txt"});
    
    willSendReplyMessage(HasSubstr("Failed to read file"));
    execute();
}

TEST_F(UpDownCommandTest, UploadExistingFile) {
    // Create a temp file for testing
    auto tempFile = std::filesystem::temp_directory_path() / "test_upload.txt";
    std::ofstream(tempFile) << "test content";
    
    setCommandExtArgs({tempFile.string()});
    
    EXPECT_CALL(*botApi, sendDocument_impl(TEST_CHAT_ID, _, _, _, _, _));
    execute();
    
    // Cleanup
    std::filesystem::remove(tempFile);
}

class DownCommandTest : public CommandTestBase {
   public:
    DownCommandTest() : CommandTestBase("down") {
        ON_CALL(strings, get(Strings::FAILED_TO_DOWNLOAD_FILE))
            .WillByDefault(Return("Failed to download"));
        ON_CALL(strings, get(Strings::OPERATION_SUCCESSFUL))
            .WillByDefault(Return("Success"));
    }
    ~DownCommandTest() override = default;
};

TEST_F(DownCommandTest, NoReplyToDocument) {
    setCommandExtArgs({"/tmp/output.txt"});
    
    willSendReplyMessage(HasSubstr("Reply to a document"));
    execute();
}

TEST_F(DownCommandTest, ReplyToDocumentSuccess) {
    setCommandExtArgs({"/tmp/output.txt"});
    
    defaultProvidedMessage->replyToMessage = createDefaultMessage();
    auto document = std::make_shared<TgBot::Document>();
    document->fileId = "doc_file_id";
    defaultProvidedMessage->replyToMessage->document = document;
    
    EXPECT_CALL(*botApi, downloadFile("/tmp/output.txt", "doc_file_id"))
        .WillOnce(Return(true));
    
    willSendReplyMessage(HasSubstr("Success"));
    execute();
}

TEST_F(DownCommandTest, ReplyToDocumentFailure) {
    setCommandExtArgs({"/tmp/output.txt"});
    
    defaultProvidedMessage->replyToMessage = createDefaultMessage();
    auto document = std::make_shared<TgBot::Document>();
    document->fileId = "doc_file_id";
    defaultProvidedMessage->replyToMessage->document = document;
    
    EXPECT_CALL(*botApi, downloadFile("/tmp/output.txt", "doc_file_id"))
        .WillOnce(Return(false));
    
    willSendReplyMessage(HasSubstr("Failed to download"));
    execute();
}
