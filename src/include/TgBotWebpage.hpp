// CLimits provides _WIN32_WINNT, do not move
// clang-format off
#include <climits>
#include <httplib.h>
// clang-format on

#include <filesystem>
#include <functional>
#include <stop_token>
#include <string_view>

#include "ManagedThreads.hpp"

class TgBotWebServerBase {
   public:
    void startServer();
    void stopServer();

    explicit TgBotWebServerBase(int serverPort,
                                std::filesystem::path serverPath,
                                std::string grpcServerAddr);

    static void loggerFn(const httplib::Request& req,
                         const httplib::Response& res);
    struct Constants {
        static constexpr const char* kWebRootNode = "/";
        static constexpr const char* kAboutPage = "/about.html";
        static constexpr const char* kAPIVotesKey = "votes";
        static constexpr const char* kBindToIp = "0.0.0.0";
        static constexpr const char* kLocalHostname = "localhost";

        // Special X-headers
        static constexpr const char* kHeaderRealIp = "X-Real-IP";
        static constexpr const char* kHeaderForwardedFor = "X-Forwarded-For";
        static constexpr const char* kHeaderClientVerify = "X-Client-Verify";
        static constexpr const char* kHeaderClientDn = "X-Client-DN";
        static constexpr const char* kHeaderClientFingerprint =
            "X-Client-Fingerprint";

        // Start: API v1 endpoints
        static constexpr const char* kAPIV1Base = "/api/v1";
        /*
         * Request: POST /api/v1/sendMessage
         * int64 chat_id
         * string text = 2;
         * FileType file_type = 3;
         * oneof {
         *   string file_data   // Small files (<4MB) sent directly
         *   string file_path   // Path on the server
         *   string file_id     // File ID of an existing file on Telegram
         * servers
         * }
         *
         * Where FileType is: One of photo, video, audio, document, sticker,
         * gif, dice (as string)
         *
         * Response: 200 OK
         * Response body:
         * Success = 0
         * TelegramApiException = 1
         * ErrorInvalidArgument = 2
         * ErrorCommandIgnored = 3
         * ErrorRuntimeError = 4
         * ErrorClientError = 5
         *
         * {
         *   bool success
         *   int code
         *   string message
         * }
         * Others: Appropriate HTTP error code
         */
        static constexpr const char* kAPIV1SendMessage = "/api/v1/sendMessage";
        static constexpr const char* kAPIKeyChatId = "chat_id";
        static constexpr const char* kAPIKeyText = "text";
        static constexpr const char* kAPIKeyFileType = "file_type";
        static constexpr const char* kAPIKeyFileData = "file_data";
        static constexpr const char* kAPIKeyFilePath = "file_path";
        static constexpr const char* kAPIKeyFileId = "file_id";

        /*
         * Request: GET /api/v1/stats
         * {
         *  status: bool
         *  uptime: {
         *      int days,
         *      int hours,
         *      int minutes,
         *      int seconds
         *   }
         *   string username,
         *   int64 user_id,
         *   string operating_system,
         * }
         * Response: 200 OK
         * Others: Appropriate HTTP error code
         */
        static constexpr const char* kAPIV1Stats = "/api/v1/stats";

        /*
         * Request: POST /api/v1/votes
         * {
         *   string votes {"up" or "down"}
         * }
         * Response: 200 OK
         * Others: Appropriate HTTP error code
         */
        static constexpr const char* kAPIVotesNode = "/api/v1/votes";

        /*
         * Request: POST /api/v1/chats
         * {
         *   int64 chat_id
         *   string chat_name
         * }
         * Will add a chat to the database if it does not already exist.
         * Will update the chat_name if it already exists.
         * If chat_name is omitted, it will be deleted from the database.
         * Response: 200 OK
         * Others: Appropriate HTTP error code
         *
         * Request: GET /api/v1/chats?chat_name={chat_name}
         * Response: 200 OK
         * Response body: { "success": bool, "chat_id": int64 }
         * Others: Appropriate HTTP error code with success=false
         */
        static constexpr const char* kAPIV1ChatsNode = "/api/v1/chats";
        static constexpr const char* kAPIKeyChatName = "chat_name";

        /*
         * Request: POST /api/v1/media
         * {
         *   string media_id
         *   listof string alias
         *   FileType media_type
         * }
         * Where FileType is: One of photo, video, audio, document, sticker,
         * gif, dice (as string)
         *
         * Will add a media file to the database if it does not already exist.
         * Will update the alias if it already exists.
         * If alias is omitted, it will be deleted from the database.
         * Response: 200 OK
         * Others: Appropriate HTTP error code
         *
         * Request: GET /api/v1/media?alias={alias}
         * Response: 200 OK
         * Response body: { "success": bool, "media_id": listof string }
         * Others: Appropriate HTTP error code with success=false
         */
        static constexpr const char* kAPIV1MediaNode = "/api/v1/media";
        static constexpr const char* kAPIKeyAlias = "alias";
        static constexpr const char* kAPIKeyMediaId = "media_id";

        /*
         * Request: GET /api/v1/hardware
         * {
         *    "success": bool,
         *    "cpu" : {
         *       "usage_percent": float,
         *       "core_count": int,
         *       "name": string
         *    },
         *    "memory" : {
         *       "total_mbytes": int64,
         *       "used_mbytes": int64,
         *    },
         *    "disk" : {
         *       "total_gbytes": int32,
         *       "used_gbytes": int32,
         *    },
         *    "os": {
         *       "name": string,
         *       "version": string,
         *       "kernel_version": string,
         *       "hostname": string,
         *       "uptime_seconds": int64
         *    }
         * }
         * Response: 200 OK
         * Others: Appropriate HTTP error code
         */
        static constexpr const char* kAPIV1Hardware = "/api/v1/hardware";
        static constexpr const char* kAPIKeyCPU = "cpu";
        static constexpr const char* kAPIKeyMemory = "memory";
        static constexpr const char* kAPIKeyDisk = "disk";
        static constexpr const char* kAPIKeyOS = "os";

        // End: API v1 endpoints
    };

   private:
    std::string _grpcServerAddr;

   public:
    struct Callbacks {
        using type = std::function<void(const httplib::Request& req,
                                        const httplib::Response& res)>;
        void showIndex(const httplib::Request& req, httplib::Response& res);
        void handleSendMessage(const httplib::Request& req,
                               httplib::Response& res);
        void handleStats(const httplib::Request& req, httplib::Response& res);
        void handleChats(const httplib::Request& req, httplib::Response& res);
        void handleMedia(const httplib::Request& req, httplib::Response& res);
        void handleChatsGet(const httplib::Request& req,
                            httplib::Response& res);
        void handleMediaGet(const httplib::Request& req,
                            httplib::Response& res);
        void handleHardware(const httplib::Request& req,
                            httplib::Response& res);
        static void handleAPIVotes(const httplib::Request& req,
                                   httplib::Response& res);
        explicit Callbacks(TgBotWebServerBase* server);
        ~Callbacks();

        struct Connection;  // gRPC Connection struct
       private:
        TgBotWebServerBase* server;
        std::unique_ptr<Connection> _conn;
    } callback;

    friend class Callbacks;

   private:
    int port;
    httplib::Server svr;
    std::filesystem::path webServerRootPath;
};

class TgBotWebServer : public ThreadRunner, public TgBotWebServerBase {
   public:
    explicit TgBotWebServer(std::filesystem::path wwwResource, int serverPort,
                            std::string grpcServerAddr);

    void runFunction(const std::stop_token& token) override;
    void onPreStop() override;
};