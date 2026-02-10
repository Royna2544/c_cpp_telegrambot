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
                                std::filesystem::path serverPath);

    static void loggerFn(const httplib::Request& req,
                         const httplib::Response& res);
    struct Constants {
        static constexpr const char* kWebRootNode = "/";
        static constexpr const char* kAboutPage = "/about.html";
        static constexpr const char* kAPIVotesKey = "votes";
        static constexpr const char* kBindToIp = "0.0.0.0";

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
         * REST: POST /api/v1/messages
         * Creates (Sends) a message.
         */
        static constexpr const char* kAPIV1Messages =
            "/api/v1/messages";  // Was sendMessage
        static constexpr const char* kAPIKeyChatId = "chat_id";
        static constexpr const char* kAPIKeyText = "text";
        static constexpr const char* kAPIKeyFileType = "file_type";
        static constexpr const char* kAPIKeyFileData = "file_data";
        static constexpr const char* kAPIKeyFilePath = "file_path";
        static constexpr const char* kAPIKeyFileId = "file_id";

        /*
         * REST: GET /api/v1/stats
         */
        static constexpr const char* kAPIV1Stats = "/api/v1/stats";

        /*
         * REST: POST /api/v1/votes
         */
        static constexpr const char* kAPIVotesNode = "/api/v1/votes";

        /*
         * REST:
         * PUT    /api/v1/chats/:chat_id  -> Set Alias (Body: {chat_name})
         * DELETE /api/v1/chats/:chat_id  -> Remove Alias
         * GET    /api/v1/chats?chat_name=X -> Search ID by Name
         */
        static constexpr const char* kAPIV1ChatsNode = "/api/v1/chats";
        static constexpr const char* kAPIKeyChatName = "chat_name";

        /*
         * REST:
         * PUT    /api/v1/media/:media_id -> Set Alias (Body: {alias, type})
         * DELETE /api/v1/media/:media_id -> Remove Alias
         * GET    /api/v1/media?alias=X   -> Search ID by Alias
         */
        static constexpr const char* kAPIV1MediaNode = "/api/v1/media";
        static constexpr const char* kAPIKeyAlias = "alias";
        static constexpr const char* kAPIKeyMediaId = "media_id";
        static constexpr const char* kAPIKeyMediaUniqueId = "media_unique_id";
        static constexpr const char* kAPIKeyMediaType = "media_type";

        /*
         * REST: GET /api/v1/hardware
         */
        static constexpr const char* kAPIV1Hardware = "/api/v1/hardware";
        static constexpr const char* kAPIKeyCPU = "cpu";
        static constexpr const char* kAPIKeyMemory = "memory";
        static constexpr const char* kAPIKeyDisk = "disk";
        static constexpr const char* kAPIKeyOS = "os";

        // End: API v1 endpoints
    };

    struct Callbacks {
        using type = std::function<void(const httplib::Request& req,
                                        const httplib::Response& res)>;
        void showIndex(const httplib::Request& req, httplib::Response& res);

        // POST /messages
        void handleMessageCreate(const httplib::Request& req,
                                 httplib::Response& res);

        // GET /stats
        void handleStats(const httplib::Request& req, httplib::Response& res);

        // Chats
        void handleChatPut(const httplib::Request& req,
                           httplib::Response& res);  // Update/Create
        void handleChatDelete(const httplib::Request& req,
                              httplib::Response& res);  // Delete
        void handleChatsGet(const httplib::Request& req,
                            httplib::Response& res);  // Search

        // Media
        void handleMediaPut(const httplib::Request& req,
                            httplib::Response& res);  // Update/Create
        void handleMediaDelete(const httplib::Request& req,
                               httplib::Response& res);  // Delete
        void handleMediaGet(const httplib::Request& req,
                            httplib::Response& res);  // Search

        // Hardware
        void handleHardware(const httplib::Request& req,
                            httplib::Response& res);

        // Votes
        static void handleAPIVotes(const httplib::Request& req,
                                   httplib::Response& res);

        explicit Callbacks(TgBotWebServerBase* server);
        ~Callbacks();

        struct Connection;

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
    explicit TgBotWebServer(std::filesystem::path wwwResource, int serverPort);

    void runFunction(const std::stop_token& token) override;
    void onPreStop() override;
};