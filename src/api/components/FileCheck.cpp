#include <absl/log/log.h>
#include <absl/strings/strip.h>
#include <curl/curl.h>
#include <fmt/chrono.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <trivial_helpers/_tgbot.h>

#include <api/components/FileCheck.hpp>
#include <chrono>
#include <filesystem>
#include <memory>
#include <optional>
#include <thread>
#include <utility>

#include "TryParseStr.hpp"
#include "api/MessageExt.hpp"
#include "libos/libsighandler.hpp"
#include "tgbot/types/CallbackQuery.h"
#include "tgbot/types/InlineKeyboardMarkup.h"

struct EngineResult {
    enum class Category {
        unknown,
        confirmed_timeout,
        timeout,
        failure,
        harmless,
        undetected,
        suspicious,
        malicious,
        type_unsupported,
    } category{};
    struct {
        std::string name;
        std::string version;
        std::string date;
    } engine;
    std::string method;
    std::string result;

    [[nodiscard]] bool virus() const {
        return category == Category::malicious ||
               category == Category::suspicious;
    }
    [[nodiscard]] bool harmless() const {
        return category == Category::harmless;
    }
    [[nodiscard]] bool idk() const { return !harmless() && !virus(); }
};

struct VirusTotalResult {
    std::vector<EngineResult> results;
    enum class Status {
        unknown,
        completed,
        pending,
        in_progress,
    } status{};

    bool verbose = false;
};

template <>
struct fmt::formatter<EngineResult::Category> : formatter<string_view> {
    // parse is inherited from formatter<string_view>.
    auto format(const EngineResult::Category &c,
                format_context &ctx) const -> format_context::iterator {
        std::string_view category = "unknown";
        switch (c) {
            case EngineResult::Category::confirmed_timeout:
            case EngineResult::Category::timeout:
                category = "T";
                break;
            case EngineResult::Category::failure:
            case EngineResult::Category::type_unsupported:
                category = "F";
                break;
            case EngineResult::Category::harmless:
                category = "O";
                break;
            case EngineResult::Category::undetected:
                category = "U";
                break;
            case EngineResult::Category::suspicious:
            case EngineResult::Category::malicious:
                category = "X";
                break;
            default:
                break;
        }
        return formatter<string_view>::format(category, ctx);
    }
};

// Specialize fmt::formatter for EngineResult
template <>
struct fmt::formatter<EngineResult> {
    static constexpr bool kVerbose = false;
    static constexpr auto parse(fmt::format_parse_context &ctx) {
        return ctx.begin();
    }

    static auto format(const EngineResult &result, fmt::format_context &ctx) {
        if constexpr (kVerbose) {
            return fmt::format_to(ctx.out(), "{} (d: {}, v: {}): {}",
                                  result.engine.name, result.engine.date,
                                  result.engine.version, result.category);
        }
        return fmt::format_to(ctx.out(), "{}: {}", result.engine.name,
                              result.category);
    }
};

// Specialize fmt::formatter for VirusTotalResult::Status
template <>
struct fmt::formatter<VirusTotalResult::Status>
    : fmt::formatter<std::string_view> {
    auto format(VirusTotalResult::Status status,
                fmt::format_context &ctx) const {
        std::string_view status_str = "unknown";
        switch (status) {
            case VirusTotalResult::Status::completed:
                status_str = "completed";
                break;
            case VirusTotalResult::Status::pending:
                status_str = "pending";
                break;
            case VirusTotalResult::Status::in_progress:
                status_str = "in_progress";
                break;
            default:
                break;
        }
        return fmt::formatter<std::string_view>::format(status_str, ctx);
    }
};

// Specialize fmt::formatter for VirusTotalResult
template <>
struct fmt::formatter<VirusTotalResult> {
    static constexpr auto parse(fmt::format_parse_context &ctx) {
        return ctx.begin();
    }

    static auto format(const VirusTotalResult &result,
                       fmt::format_context &ctx) {
        std::string nonverboseout = fmt::format(
            "VirusTotal scan result (status: {}):\nSummary: "
            "Virus: {}, Not Virus: {}, IDK: {}",
            result.status,
            std::ranges::count_if(result.results,
                                  [](const auto &res) { return res.virus(); }),
            std::ranges::count_if(
                result.results, [](const auto &res) { return res.harmless(); }),
            std::ranges::count_if(result.results,
                                  [](const auto &res) { return res.idk(); }));

        if (result.verbose) {
            return fmt::format_to(ctx.out(),
                                  "{}\nLegend: T: timeout, F: AV failure, O: "
                                  "harmless, X: malicious, U: unknown\n- {}",
                                  nonverboseout,
                                  fmt::join(result.results, "\n- "));
        }
        return fmt::format_to(ctx.out(), "{}", nonverboseout);
    }
};

// Specialize fmt::formatter for VirusTotalResult
template <typename Req, typename Ret>
class API {
   protected:
    // Parse the response from virustotal API
    static size_t callback(void *contents, size_t size, size_t nmemb,
                           void *userp) {
        std::string s((char *)contents, size * nmemb);
        *static_cast<callback_payload_type>(userp) += s;
        return size * nmemb;
    }

   public:
    using callback_payload_type = std::string *;
    using request_type = Req;
    using return_type = Ret;

    virtual ~API() = default;

    static constexpr int HTTP_OK = 200;
    static constexpr int HTTP_BAD_REQUEST = 400;

    // Send a request to virustotal API
    virtual std::optional<Json::Value> request(const request_type &path) = 0;
    virtual std::optional<return_type> parseResponse(
        const Json::Value &response) = 0;
};

class SendFileAPI : public API<std::filesystem::path, std::string> {
    std::string _token;

   public:
    ~SendFileAPI() override = default;
    explicit SendFileAPI(std::string token) : _token(std::move(token)) {}

    static constexpr std::string_view URL =
        "https://www.virustotal.com/api/v3/files";

    // Send a request to virustotal API
    std::optional<Json::Value> request(const request_type &path) override;
    std::optional<return_type> parseResponse(
        const Json::Value &response) override;
};

class GetAnalysisAPI : public API<std::string, VirusTotalResult> {
    std::string _token;

   public:
    ~GetAnalysisAPI() override = default;
    explicit GetAnalysisAPI(std::string token) : _token(std::move(token)) {}

    // Send a request to virustotal API
    std::optional<Json::Value> request(const request_type &path) override;
    std::optional<return_type> parseResponse(
        const Json::Value &response) override;
};

std::optional<Json::Value> SendFileAPI::request(const request_type &path) {
    CURL *curl = nullptr;
    CURLcode res{};
    struct curl_slist *headers = nullptr;
    std::string result;

    // Initialize cURL
    curl = curl_easy_init();
    if (curl != nullptr) {
        // Set the URL of the API endpoint
        curl_easy_setopt(curl, CURLOPT_URL, URL.data());

        // Add API key to headers
        std::string apiKey = fmt::format("x-apikey: {}", _token);
        headers = curl_slist_append(headers, apiKey.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Specify the file to upload
        curl_mime *mime = nullptr;
        curl_mimepart *part = nullptr;

        // Create the mime structure
        mime = curl_mime_init(curl);

        // Add the file part
        part = curl_mime_addpart(mime);
        curl_mime_name(part, "file");
        curl_mime_filedata(part, path.string().c_str());

        // Attach the mime structure to the request
        curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

        // Write the response
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

        // Set the callback function for handling the response
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);

        // Perform the request
        res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK) {
            LOG(ERROR) << "cURL error: " << curl_easy_strerror(res);
        } else {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            switch (http_code) {
                case HTTP_OK:  // HTTP 200 OK
                    break;
                case HTTP_BAD_REQUEST:  // HTTP 400 Bad Request
                    LOG(ERROR) << fmt::format(
                        "Failed to send file: HTTP status code {}", http_code);
                    break;
                default:
                    LOG(ERROR)
                        << fmt::format("Unexpected status code: {}", http_code);
                    break;
            }
        }

        // Cleanup
        curl_mime_free(mime);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    } else {
        LOG(ERROR) << "Failed to initialize cURL";
        return std::nullopt;
    }

    Json::Reader reader;
    Json::Value value;
    if (reader.parse(result, value)) {
        return value;
    } else {
        return std::nullopt;
    }
}

std::optional<SendFileAPI::return_type> SendFileAPI::parseResponse(
    const Json::Value &response) {
    if (!response["data"]["links"]["self"].empty()) {
        DLOG(INFO) << "URL: " << response["data"]["links"]["self"].asString();
        return response["data"]["links"]["self"].asString();
    } else {
        return std::nullopt;
    }
}

std::optional<Json::Value> GetAnalysisAPI::request(const request_type &url) {
    CURL *curl = nullptr;
    CURLcode res{};
    struct curl_slist *headers = nullptr;
    std::string result;

    // Initialize cURL
    curl = curl_easy_init();
    if (curl != nullptr) {
        // Set the URL of the API endpoint
        curl_easy_setopt(curl, CURLOPT_URL, url.data());

        // Add API key to headers
        std::string apiKey = fmt::format("x-apikey: {}", _token);
        headers = curl_slist_append(headers, apiKey.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        // Write the response
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

        // Set the callback function for handling the response
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, callback);

        // Perform the request
        res = curl_easy_perform(curl);

        // Check for errors
        if (res != CURLE_OK) {
            LOG(ERROR) << "cURL error: " << curl_easy_strerror(res);
            return std::nullopt;
        } else {
            long http_code = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
            switch (http_code) {
                case HTTP_OK:
                    break;
                case HTTP_BAD_REQUEST:
                    LOG(ERROR) << fmt::format(
                        "Failed to send file: HTTP status code {}", http_code);
                    break;
                default:
                    LOG(ERROR)
                        << fmt::format("Unexpected status code: {}", http_code);
                    break;
            }
        }

        // Cleanup
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    } else {
        LOG(ERROR) << "Failed to initialize cURL";
        return std::nullopt;
    }

    Json::Reader reader;
    Json::Value value;
    if (reader.parse(result, value)) {
        return value;
    } else {
        return std::nullopt;
    }
}

std::optional<GetAnalysisAPI::return_type> GetAnalysisAPI::parseResponse(
    const Json::Value &response) {
    auto resultsData = response["data"];
    auto attributesNode = resultsData["attributes"];
    if (!attributesNode.empty()) {
        VirusTotalResult virusresult;
        auto statusString = attributesNode["status"].asString();
        if (statusString == "completed") {
            virusresult.status = VirusTotalResult::Status::completed;
        } else if (statusString == "queued") {
            virusresult.status = VirusTotalResult::Status::pending;
        } else if (statusString == "in-progress") {
            virusresult.status = VirusTotalResult::Status::in_progress;
        } else {
            LOG(WARNING) << "Unknown status: " << statusString;
            return std::nullopt;
        }

        if (virusresult.status != VirusTotalResult::Status::completed) {
            LOG(WARNING) << fmt::format("Not completed yet: {}",
                                        virusresult.status);
            return std::nullopt;
        }

        auto resultsNode = attributesNode["results"];
        for (const auto &items : resultsNode) {
            EngineResult result;

            auto categoryString = items["category"].asString();
            if (categoryString == "confirmed-timeout") {
                result.category = EngineResult::Category::confirmed_timeout;
            } else if (categoryString == "timeout") {
                result.category = EngineResult::Category::timeout;
            } else if (categoryString == "failure") {
                result.category = EngineResult::Category::failure;
            } else if (categoryString == "harmless") {
                result.category = EngineResult::Category::harmless;
            } else if (categoryString == "undetected") {
                result.category = EngineResult::Category::undetected;
            } else if (categoryString == "suspicious") {
                result.category = EngineResult::Category::suspicious;
            } else if (categoryString == "malicious") {
                result.category = EngineResult::Category::malicious;
            } else if (categoryString == "type-unsupported") {
                result.category = EngineResult::Category::type_unsupported;
            } else {
                LOG(WARNING) << "Unknown category: " << categoryString;
                continue;
            }

            result.engine.name = items["engine_name"].asString();
            result.engine.version = items["engine_version"].asString();
            result.engine.date = items["engine_update"].asString();
            result.method = items["method"].asString();
            result.result = items["result"].asString();
            virusresult.results.emplace_back(std::move(result));
        }
        return virusresult;
    } else {
        LOG(WARNING) << "No results found in response";
        LOG(WARNING) << response.toStyledString();
        return std::nullopt;
    }
}

TgBotApi::AnyMessageResult FileCheck::onAnyMessage(
    TgBotApi::CPtr api, const MessageExt::Ptr &message) {
    if (message->any({MessageAttrs::Animation, MessageAttrs::Photo,
                      MessageAttrs::Sticker, MessageAttrs::Video}) ||
        !message->has<MessageAttrs::Document>()) {
        return TgBotApi::AnyMessageResult::Handled;
    }
    std::filesystem::path filePath =
        std::filesystem::temp_directory_path() / "virustotal.bin";
    DLOG(INFO) << fmt::format("Received file: by {} in chat {}",
                              message->get<MessageAttrs::User>(),
                              message->get<MessageAttrs::Chat>());

    LOG(INFO) << "Downloading file";
    // Download the file using the TgBotApi::downloadFile function
    if (!api->downloadFile(filePath,
                           message->get<MessageAttrs::Document>()->fileId)) {
        LOG(ERROR) << "Failed to download file";
        return TgBotApi::AnyMessageResult::Handled;
    }
    LOG(INFO) << "Downloaded file";

    // Check the file
    SendFileAPI sendFileAPI(_token);
    auto sendFileRes = sendFileAPI.request(filePath);

    LOG(INFO) << "Sending file to VirusTotal API server";
    if (!sendFileRes) {
        LOG(ERROR) << "Failed to send file to VirusTotal API";
        return TgBotApi::AnyMessageResult::Handled;
    }
    LOG(INFO) << "Successfully sent file";

    auto sendFileProcRes = sendFileAPI.parseResponse(sendFileRes.value());
    if (!sendFileProcRes) {
        LOG(ERROR) << "Failed to parse response from VirusTotal API";
        return TgBotApi::AnyMessageResult::Handled;
    }

    GetAnalysisAPI getAnalysis(_token);
    std::optional<GetAnalysisAPI::return_type> analysisResult;
    while (!analysisResult && !SignalHandler::isSignaled()) {
        LOG(INFO) << "Getting analysis results";
        auto analfut = getAnalysis.request(sendFileProcRes.value());
        if (!analfut) {
            LOG(ERROR) << "Failed to request analysis results";
            return TgBotApi::AnyMessageResult::Handled;
        }

        analysisResult = getAnalysis.parseResponse(analfut.value());
        if (!analysisResult) {
            LOG(ERROR) << "Failed to get analysis results";
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
    LOG(INFO) << "Analysis results received";

    // Append data to the vec
    ResultHolder holder;
    auto virus = analysisResult.value();
    holder.result = fmt::format("{}", virus);
    virus.verbose = true;
    holder.verboseResult = fmt::format("{}", virus);
    holder.verbose_state = false;
    holder.summary =
        std::make_shared<TgBot::InlineKeyboardMarkup>(viewShortKeyboard);
    holder.all =
        std::make_shared<TgBot::InlineKeyboardMarkup>(viewFullyKeyboard);
    holder.summary->inlineKeyboard[0][0]->callbackData =
        holder.all->inlineKeyboard[0][0]->callbackData =
            fmt::format("{}{}", kQueryDataPrefix, counter);

    // Send the analysis result
    holder.message =
        api->sendReplyMessage(message->message(), holder.result, holder.all);

    _resultHolder[counter] = holder;
    counter++;

    // Delete the downloaded file
    std::filesystem::remove(filePath);

    return TgBotApi::AnyMessageResult::Handled;
}

void FileCheck::onCallbackQueryFunction(
    const TgBot::CallbackQuery::Ptr &query) {
    absl::string_view id = query->data;
    if (!absl::ConsumePrefix(&id, kQueryDataPrefix)) {
        return;
    }
    int counter = 0;
    if (!try_parse(id, &counter)) {
        return;
    }
    if (!_resultHolder.contains(counter)) {
        return;
    }
    auto &holder = _resultHolder[counter];
    holder.verbose_state = !holder.verbose_state;
    _api->editMessage(
        holder.message,
        holder.verbose_state ? holder.verboseResult : holder.result,
        holder.verbose_state ? holder.summary : holder.all);
}

FileCheck::FileCheck(TgBotApi::Ptr api, std::string virusTotalToken)
    : _token(std::move(virusTotalToken)), _api(api) {
    api->onAnyMessage([this](TgBotApi::CPtr api, Message::Ptr message) {
        return onAnyMessage(api, std::make_shared<MessageExt>(std::move(message)));
    });
    api->onCallbackQuery("__builtin_filecheck_handler__",
                         [this](const TgBot::CallbackQuery::Ptr &query) {
                             onCallbackQueryFunction(query);
                         });
    viewShortKeyboard.inlineKeyboard.emplace_back();
    viewShortKeyboard.inlineKeyboard[0].emplace_back(
        std::make_shared<TgBot::InlineKeyboardButton>());
    viewShortKeyboard.inlineKeyboard[0][0]->text = "View short";

    viewFullyKeyboard.inlineKeyboard.emplace_back();
    viewFullyKeyboard.inlineKeyboard[0].emplace_back(
        std::make_shared<TgBot::InlineKeyboardButton>());
    viewFullyKeyboard.inlineKeyboard[0][0]->text = "View all";
}