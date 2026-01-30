#include <grpcpp/grpcpp.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "Socket_service.grpc.pb.h"  // Generated header

using grpc::Channel;
using grpc::ClientContext;
using grpc::ClientReaderWriter;
using grpc::Status;
using namespace tgbot::proto::socket;

// Configuration
const std::string SERVER_ADDRESS = "127.0.0.1:50000";  // Or "localhost:50051"
const size_t CHUNK_SIZE = 1024 * 64;                   // 64KB

class BotClient {
   public:
    BotClient(std::shared_ptr<Channel> channel)
        : stub_(SocketService::NewStub(channel)) {}

    // ======================================================
    // Command: send_message <chat_id> <text> [file_path]
    // ======================================================
    void SendMessage(int64_t chat_id, const std::string& text,
                     const std::string& file_path = "") {
        ClientContext context;
        SendMessageRequest request;
        GenericResponse response;

        request.set_chat_id(chat_id);
        if (!text.empty()) request.set_text(text);

        if (!file_path.empty()) {
            if (std::filesystem::exists(file_path)) {
                request.set_file_path(file_path);
                request.set_file_type(FileType::DOCUMENT);  // Defaulting to Doc
            } else {
                std::cerr << "Error: File not found: " << file_path
                          << std::endl;
                return;
            }
        }

        Status status = stub_->sendMessage(&context, request, &response);
        PrintResponse(status, response);
    }

    // ======================================================
    // Command: spam_config <mode_int>
    // 0=Disabled, 1=Log, 2=Purge, 3=Purge+Mute
    // ======================================================
    void SetSpamConfig(int mode) {
        ClientContext context;
        SpamBlockingConfig request;
        GenericResponse response;

        if (mode < 0 || mode > 3) {
            std::cerr << "Invalid mode. Use 0-3." << std::endl;
            return;
        }
        request.set_mode(static_cast<SpamBlockingModes>(mode));

        Status status =
            stub_->setSpamBlockingConfig(&context, request, &response);
        PrintResponse(status, response);
    }

    // ======================================================
    // Command: info
    // ======================================================
    void GetInfo() {
        ClientContext context;
        google::protobuf::Empty request;
        BotInfo response;

        Status status = stub_->info(&context, request, &response);
        if (status.ok()) {
            std::cout << "Bot Info:" << std::endl;
            std::cout << "  Username: " << response.username() << std::endl;
            std::cout << "  ID: " << response.user_id() << std::endl;
            std::cout << "  OS: " << response.operating_system() << std::endl;
            std::cout << "  Uptime: " << response.uptime().days() << "d "
                      << response.uptime().hours() << "h" << std::endl;
        } else {
            std::cerr << "RPC Failed: " << status.error_message() << std::endl;
        }
    }

    // ======================================================
    // Command: upload <local_path> <remote_path>
    // ======================================================
    void UploadFile(const std::string& local_path,
                    const std::string& remote_path) {
        if (!std::filesystem::exists(local_path)) {
            std::cerr << "Local file not found." << std::endl;
            return;
        }

        // STEP 1: Handshake
        ClientContext ctx1;
        FileTransferRequest req;
        FileTransferResponse resp;

        req.set_file_path(remote_path);  // Where to save on server
        req.set_is_upload(true);
        req.set_overwrite_existing(true);
        req.set_file_size(std::filesystem::file_size(local_path));

        Status status = stub_->requestFileTransfer(&ctx1, req, &resp);
        if (!status.ok() || !resp.accepted()) {
            std::cerr << "Upload handshake failed: " << status.error_message()
                      << std::endl;
            return;
        }
        std::string uuid = resp.uuid();
        std::cout << "Handshake OK. UUID: " << uuid << std::endl;

        // STEP 2: Streaming Loop
        ClientContext ctx2;
        std::shared_ptr<ClientReaderWriter<FileChunk, FileChunkResponse>>
            stream(stub_->uploadFileLoop(&ctx2));

        std::ifstream infile(local_path, std::ios::binary);
        std::vector<char> buffer(CHUNK_SIZE);
        int chunk_idx = 0;
        size_t offset = 0;

        while (infile.read(buffer.data(), buffer.size()) ||
               infile.gcount() > 0) {
            FileChunk chunk;
            chunk.set_uuid(uuid);
            chunk.set_chunk_idx(chunk_idx++);
            chunk.set_chunk_data(buffer.data(), infile.gcount());
            chunk.set_chunk_offset(static_cast<int64_t>(offset));

            if (!stream->Write(chunk)) {
                std::cerr << "Stream broken!" << std::endl;
                break;
            }
            offset += infile.gcount();

            // Optional: Read ACK (if your server sends one per chunk)
            FileChunkResponse ack;
            stream->Read(&ack);
            if (!ack.success()) {
                std::cerr << "Chunk upload failed, retrying..." << std::endl;
                // In production, implement retry logic here
            }
        }

        stream->WritesDone();
        Status stream_status = stream->Finish();
        if (!stream_status.ok()) {
            std::cerr << "Stream failed: " << stream_status.error_message()
                      << std::endl;
            return;
        }

        // STEP 3: Finalize
        ClientContext ctx3;
        FileTransferRequest end_req;
        GenericResponse end_resp;
        end_req.set_uuid(uuid);
        end_req.set_file_path(remote_path);
        end_req.set_is_upload(true);

        stub_->endFileTransfer(&ctx3, end_req, &end_resp);
        PrintResponse(Status::OK, end_resp);
    }

    // ======================================================
    // Command: download <remote_path> <local_path>
    // ======================================================
    void DownloadFile(const std::string& remote_path,
                      const std::string& local_path) {
        // STEP 1: Handshake
        ClientContext ctx1;
        FileTransferRequest req;
        FileTransferResponse resp;

        req.set_file_path(remote_path);
        req.set_is_upload(false);

        Status status = stub_->requestFileTransfer(&ctx1, req, &resp);
        if (!status.ok() || !resp.accepted()) {
            std::cerr << "Download handshake failed or rejected." << std::endl;
            return;
        }

        std::string uuid = resp.uuid();
        int32_t total_chunks = resp.chunk_count();
        int64_t file_size = resp.file_size();
        std::cout << "Downloading " << file_size << " bytes (" << total_chunks
                  << " chunks)..." << std::endl;

        // STEP 2: Streaming Loop
        ClientContext ctx2;
        std::shared_ptr<ClientReaderWriter<FileChunkRequest, FileChunkResponse>>
            stream(stub_->downloadFileLoop(&ctx2));

        std::ofstream outfile(local_path, std::ios::binary);

        // Pipelining strategy: Request all chunks (simplified)
        // In production, maintain a 'window' of active requests.
        for (int i = 0; i < total_chunks; ++i) {
            FileChunkRequest chunk_req;
            chunk_req.set_uuid(uuid);
            chunk_req.set_chunk_idx(i);

            if (!stream->Write(chunk_req)) break;

            FileChunkResponse chunk_resp;
            if (stream->Read(&chunk_resp) && chunk_resp.success()) {
                outfile.write(chunk_resp.chunk().chunk_data().data(),
                              chunk_resp.chunk().chunk_data().size());

                if (i % 10 == 0)
                    std::cout << "\rChunk " << i << "/" << total_chunks
                              << std::flush;
            } else {
                std::cerr << "Failed to read chunk " << i << std::endl;
                break;
            }
        }
        std::cout << std::endl;

        stream->WritesDone();
        stream->Finish();
        outfile.close();

        // STEP 3: Finalize
        ClientContext ctx3;
        FileTransferRequest end_req;
        GenericResponse end_resp;
        end_req.set_uuid(uuid);
        end_req.set_file_path(remote_path);
        end_req.set_is_upload(false);

        stub_->endFileTransfer(&ctx3, end_req, &end_resp);
        PrintResponse(Status::OK, end_resp);
    }

   private:
    std::unique_ptr<SocketService::Stub> stub_;

    void PrintResponse(const Status& status, const GenericResponse& resp) {
        if (status.ok()) {
            std::cout << "Success: " << resp.message() << std::endl;
        } else {
            std::cerr << "RPC Error " << status.error_code() << ": "
                      << status.error_message() << std::endl;
        }
    }
};

int app_main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: ./client <dest> <command> [args...]\n"
                  << "Commands:\n"
                  << "  send_message <chat_id> <text> [file_path]\n"
                  << "  spam_config <0-3>\n"
                  << "  info\n"
                  << "  upload <local_path> <remote_path>\n"
                  << "  download <remote_path> <local_path>\n";
        return 1;
    }

    // Connect to server
    auto channel =
        grpc::CreateChannel(argv[1], grpc::InsecureChannelCredentials());
    BotClient client(channel);

    argc--;
    argv++;
    std::string command = argv[1];

    try {
        if (command == "send_message") {
            if (argc < 4) throw std::runtime_error("Missing args");
            int64_t chat_id = std::stoll(argv[2]);
            std::string text = argv[3];
            std::string file = (argc >= 5) ? argv[4] : "";
            client.SendMessage(chat_id, text, file);
        } else if (command == "spam_config") {
            if (argc < 3) throw std::runtime_error("Missing mode");
            client.SetSpamConfig(std::stoi(argv[2]));
        } else if (command == "info") {
            client.GetInfo();
        } else if (command == "upload") {
            if (argc < 4) throw std::runtime_error("Missing paths");
            client.UploadFile(argv[2], argv[3]);
        } else if (command == "download") {
            if (argc < 4) throw std::runtime_error("Missing paths");
            client.DownloadFile(argv[2], argv[3]);
        } else {
            std::cerr << "Unknown command: " << command << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error parsing arguments: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}