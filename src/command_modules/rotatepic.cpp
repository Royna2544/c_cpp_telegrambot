#include <absl/log/log.h>

#include <TryParseStr.hpp>
#include <algorithm>
#include <api/CommandModule.hpp>
#include <api/StringResLoader.hpp>
#include <api/TgBotApi.hpp>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <imagep/ImageProcAll.hpp>
#include <imagep/MediaLimits.hpp>
#include <limits>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <system_error>

#include "ImagePBase.hpp"
#include "api/MessageExt.hpp"

#ifdef _WIN32
#include <Windows.h>
#include <sddl.h>
#else
#include <sys/stat.h>
#endif

struct ProcessImageParam {
    std::filesystem::path workPath;
    std::filesystem::path srcPath;
    std::filesystem::path destPath;
    std::string mimeType;
    PhotoBase::Target target;
    PhotoBase::Options options;

    ~ProcessImageParam() {
        std::error_code ignored;
        if (!workPath.empty()) {
            std::filesystem::remove_all(workPath, ignored);
        }
    }
};

namespace {

constexpr auto kMediaDeadline = std::chrono::seconds(60);
constexpr std::size_t kTempDirectoryAttempts = 32;
std::atomic<std::uint64_t> tempDirectorySequence{0};

bool createOwnerOnlyDirectory(const std::filesystem::path& path) {
#ifdef _WIN32
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(A;;FA;;;OW)", SDDL_REVISION_1, &descriptor, nullptr)) {
        return false;
    }
    SECURITY_ATTRIBUTES attributes{
        .nLength = sizeof(attributes),
        .lpSecurityDescriptor = descriptor,
        .bInheritHandle = FALSE,
    };
    const bool created = CreateDirectoryW(path.c_str(), &attributes) != FALSE;
    LocalFree(descriptor);
    return created;
#else
    return mkdir(path.c_str(), S_IRWXU) == 0;
#endif
}

std::optional<std::filesystem::path> createPrivateTempDirectory(
    const std::filesystem::path& tempRoot, RandomBase* random) {
    if (random == nullptr) {
        return std::nullopt;
    }
    try {
        for (std::size_t attempt = 0; attempt < kTempDirectoryAttempts;
             ++attempt) {
            const auto randomHigh = random->generate(
                0, std::numeric_limits<RandomBase::ret_type>::max());
            const auto randomLow = random->generate(
                0, std::numeric_limits<RandomBase::ret_type>::max());
            const auto sequence =
                tempDirectorySequence.fetch_add(1, std::memory_order_relaxed);
            const auto candidate =
                tempRoot /
                ("glider_rotatepic_" + std::to_string(randomHigh) + "_" +
                 std::to_string(randomLow) + "_" + std::to_string(sequence));
            if (createOwnerOnlyDirectory(candidate)) {
                return candidate;
            }
        }
    } catch (const std::exception& ex) {
        LOG(ERROR) << "Failed to create private media workspace: " << ex.what();
    }
    return std::nullopt;
}

bool processingExpired(
    std::stop_token stop,
    std::chrono::steady_clock::time_point deadline) noexcept {
    return stop.stop_requested() ||
           std::chrono::steady_clock::now() >= deadline;
}

PhotoBase::TinyStatus processFile(
    ProcessImageParam& param, std::stop_token stop,
    std::chrono::steady_clock::time_point deadline) {
    try {
        if (processingExpired(stop, deadline)) {
            return {PhotoBase::Status::kProcessingError,
                    "Media processing cancelled or deadline exceeded"};
        }
        ImageProcessingAll procAll(param.srcPath);
        if (!procAll.read(param.target, stop, deadline)) {
            return {PhotoBase::Status::kReadError,
                    "No backend suitable for reading"};
        }
        procAll.options = param.options;
        auto result = procAll.processAndWrite(param.destPath, stop, deadline);
        if (!result.isOk()) {
            return result;
        }
        std::error_code ec;
        const auto outputBytes = std::filesystem::file_size(param.destPath, ec);
        if (ec || outputBytes > imagep::limits::kMaxOutputBytes) {
            return {PhotoBase::Status::kWriteError,
                    "Processed output exceeds the size limit"};
        }
        return result;
    } catch (const std::exception& ex) {
        LOG(ERROR) << "Media processing failed: " << ex.what();
        return {PhotoBase::Status::kProcessingError, "Media processing failed"};
    } catch (...) {
        LOG(ERROR) << "Media processing failed with an unknown exception";
        return {PhotoBase::Status::kProcessingError, "Media processing failed"};
    }
}

constexpr std::string_view kDownloadFile = "inpic";
constexpr std::string_view kOutputFile = "outpic";

void rotatepicWork(TgBotApi::Ptr api, MessageExt::Ptr message,
                   const StringResLoader::PerLocaleMap* res, RandomBase* random,
                   std::stop_token stop,
                   std::chrono::steady_clock::time_point deadline) {
    if (processingExpired(stop, deadline)) {
        return;
    }
    int rotation = 0;
    ProcessImageParam params{};
    std::error_code ec;
    auto tmpPath = std::filesystem::temp_directory_path(ec);
    if (ec) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::FAILED_TO_DOWNLOAD_FILE));
        return;
    }
    std::string fileid;
    enum class MediaType {
        Invalid,
        PhotoJpeg,
        StickerWebp,
        VideoMp4,
        VideoWebm,
        GifToMp4,
    } mediaType{};
    MessageAttrs attr{};
    std::int64_t width = 0;
    std::int64_t height = 0;
    std::int64_t duration = 0;
    std::optional<std::int64_t> declaredFileSize;
    bool isVideo = false;

    auto replyMessage = message->reply();

    if (replyMessage->has<MessageAttrs::Photo>()) {
        const auto photo = replyMessage->get<MessageAttrs::Photo>();
        fileid = photo->fileId;
        width = photo->width;
        height = photo->height;
        if (photo->fileSize) {
            declaredFileSize = *photo->fileSize;
        }
        mediaType = MediaType::PhotoJpeg;
        attr = MessageAttrs::Photo;
    } else if (replyMessage->has<MessageAttrs::Sticker>()) {
        const auto stick = replyMessage->get<MessageAttrs::Sticker>();
        if (stick->isAnimated) {
            // Animated stickers are gzipped Lottie/TGS documents, not video.
            api->sendReplyMessage(message->message(),
                                  res->get(Strings::CANNOT_ROTATE_NONSTATIC));
            return;
        }
        if (stick->isVideo) {
#ifndef IMAGEPROC_HAVE_OPENCV
            api->sendReplyMessage(message->message(),
                                  res->get(Strings::CANNOT_ROTATE_NONSTATIC));
            return;
#endif
            mediaType = MediaType::VideoWebm;
            isVideo = true;
        } else {
            mediaType = MediaType::StickerWebp;
        }
        fileid = stick->fileId;
        width = stick->width;
        height = stick->height;
        if (stick->fileSize) {
            declaredFileSize = *stick->fileSize;
        }
        attr = MessageAttrs::Sticker;
#ifdef IMAGEPROC_HAVE_OPENCV
    } else if (replyMessage->has<MessageAttrs::Animation>()) {
        const auto animation = replyMessage->get<MessageAttrs::Animation>();
        fileid = animation->fileId;
        width = animation->width;
        height = animation->height;
        duration = animation->duration;
        declaredFileSize = animation->fileSize;
        mediaType = animation->mimeType == "image/gif" ? MediaType::GifToMp4
                                                       : MediaType::VideoMp4;
        isVideo = true;
        attr = MessageAttrs::Animation;
    } else if (replyMessage->has<MessageAttrs::Video>()) {
        const auto video = replyMessage->get<MessageAttrs::Video>();
        fileid = video->fileId;
        width = video->width;
        height = video->height;
        duration = video->duration;
        declaredFileSize = video->fileSize;
        mediaType = MediaType::VideoMp4;
        isVideo = true;
        attr = MessageAttrs::Video;
#endif
    } else {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::REPLY_TO_A_MEDIA));
        return;
    }

    const bool dimensionsAllowed =
        isVideo ? imagep::limits::videoDimensionsAllowed(width, height)
                : imagep::limits::imageDimensionsAllowed(width, height);
    const bool durationAllowed =
        !isVideo || (duration >= 0 && static_cast<std::uint64_t>(duration) <=
                                          imagep::limits::kMaxDurationSeconds);
    const bool fileSizeAllowed =
        !declaredFileSize ||
        imagep::limits::compressedSizeAllowed(*declaredFileSize);
    if (!dimensionsAllowed || !durationAllowed || !fileSizeAllowed) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::FAILED_TO_ROTATE_IMAGE));
        return;
    }

    auto args = message->get<MessageAttrs::ParsedArgumentsList>();
    if (!try_parse(args[0], &rotation)) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::INVALID_ANGLE));
        return;
    }
    constexpr std::array kSupportedAngles{
        PhotoBase::kAngleMin, PhotoBase::kAngle90, PhotoBase::kAngle180,
        PhotoBase::kAngle270};
    if (std::ranges::find(kSupportedAngles, rotation) ==
        kSupportedAngles.end()) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::INVALID_ANGLE));
        return;
    }
    if (args.size() == 2) {
        if (args[1] != "greyscale" && args[1] != "invert") {
            api->sendReplyMessage(message->message(),
                                  res->get(Strings::INVALID_ARGS_PASSED));
            return;
        }
        params.options.greyscale = args[1] == "greyscale";
        params.options.invert_color = args[1] == "invert";
    }
    params.options.rotate_angle = rotation;

    auto privateTempPath = createPrivateTempDirectory(tmpPath, random);
    if (!privateTempPath) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::FAILED_TO_DOWNLOAD_FILE));
        return;
    }
    params.workPath = std::move(*privateTempPath);
    params.srcPath = params.workPath / kDownloadFile;
    params.destPath = params.workPath / kOutputFile;

    // Process the image
    switch (mediaType) {
        case MediaType::VideoWebm: {
            params.srcPath.replace_extension(".webm");
            params.destPath.replace_extension(".webm");
            params.mimeType = "video/webm";
            params.target = PhotoBase::Target::kVideo;
        } break;
        case MediaType::VideoMp4: {
            params.srcPath.replace_extension(".mp4");
            params.destPath.replace_extension(".mp4");
            params.mimeType = "video/mp4";
            params.target = PhotoBase::Target::kVideo;
        } break;
        case MediaType::GifToMp4: {
            params.srcPath.replace_extension(".gif");
            params.destPath.replace_extension(".mp4");
            params.mimeType = "video/mp4";
            params.target = PhotoBase::Target::kVideo;
        } break;
        case MediaType::PhotoJpeg: {
            params.mimeType = "image/jpeg";
            params.srcPath.replace_extension(".jpg");
            params.destPath.replace_extension(".jpg");
            params.target = PhotoBase::Target::kPhoto;
        } break;
        case MediaType::StickerWebp: {
            params.mimeType = "image/webp";
            params.srcPath.replace_extension(".webp");
            params.destPath.replace_extension(".webp");
            params.target = PhotoBase::Target::kPhoto;
        } break;
        case MediaType::Invalid:
            return;
    }

    // Download the sticker file
    if (!api->downloadFileWithinLimit(
            params.srcPath, fileid, imagep::limits::kMaxCompressedInputBytes)) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::FAILED_TO_DOWNLOAD_FILE));
        return;
    }
    if (processingExpired(stop, deadline)) {
        return;
    }
    const auto compressedBytes = std::filesystem::file_size(params.srcPath, ec);
    if (ec || compressedBytes > imagep::limits::kMaxCompressedInputBytes) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::FAILED_TO_ROTATE_IMAGE));
        return;
    }

    const auto ret = processFile(params, stop, deadline);
    if (processingExpired(stop, deadline)) {
        return;
    }
    if (ret.isOk()) {
        const auto infile = TgBot::InputFile::fromFile(params.destPath.string(),
                                                       params.mimeType);
        switch (attr) {
            case MessageAttrs::Photo:
                api->sendReplyPhoto(message->message(), infile,
                                    res->get(Strings::ROTATED_PICTURE));
                break;
            case MessageAttrs::Sticker:
                api->sendReplySticker(message->message(), infile);
                break;
            case MessageAttrs::Animation:
                api->sendReplyAnimation(message->message(), infile,
                                        res->get(Strings::ROTATED_PICTURE));
                break;
            case MessageAttrs::Video:
                api->sendReplyVideo(message->message(), infile);
                break;
            default:
                // Not possible
                break;
        }
    } else {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::FAILED_TO_ROTATE_IMAGE));
        api->sendReplyMessage(message->message(), ret.getMessage());
    }
}

DECLARE_COMMAND_HANDLER(rotatepic) {
    auto ownedMessage = std::make_shared<MessageExt>(*message);
    auto* random = provider->random.get();
    const auto deadline = std::chrono::steady_clock::now() + kMediaDeadline;
    if (!api->submitCommandWork(
            "rotatepic", TgBotApi::WorkClass::Media,
            [api, res, random, ownedMessage = std::move(ownedMessage),
             deadline](std::stop_token stop) {
                rotatepicWork(api, ownedMessage.get(), res, random, stop,
                              deadline);
            },
            {.deadline = kMediaDeadline})) {
        api->sendReplyMessage(
            message->message(),
            "The media processing queue is full. Please retry later.");
    }
}
}  // namespace

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "rotatepic",
    .description = "Rotate a sticker/video/photo",
    .function = COMMAND_HANDLER_NAME(rotatepic),
    .valid_args =
        {
            .enabled = true,
            .counts = DynModule::craftArgCountMask<1, 2>(),
            .split_type = DynModule::ValidArgs::Split::ByWhitespace,
            .usage = "/rotatepic angle [greyscale|invert]",
        },
};
