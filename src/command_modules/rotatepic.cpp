#include <absl/status/status.h>

#include <TryParseStr.hpp>
#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>
#include <cctype>
#include <filesystem>
#include <imagep/ImageProcAll.hpp>
#include <string>
#include <string_view>
#include <system_error>

#include "ImagePBase.hpp"
#include "StringResLoader.hpp"
#include "api/MessageExt.hpp"

struct ProcessImageParam {
    std::filesystem::path srcPath;
    std::filesystem::path destPath;
    std::string mimeType;
    PhotoBase::Target target;
    PhotoBase::Options options;
};

namespace {
absl::Status processFile(ProcessImageParam& param) {
    ImageProcessingAll procAll(param.srcPath);
    if (procAll.read(param.target)) {
        procAll.options = param.options;
        return procAll.processAndWrite(param.destPath);
    }
    return absl::InternalError("No backend suitable for reading");
}

constexpr std::string_view kDownloadFile = "inpic";
constexpr std::string_view kOutputFile = "outpic";

DECLARE_COMMAND_HANDLER(rotatepic) {
    int rotation = 0;
    ProcessImageParam params{};
    std::error_code ec;
    auto tmpPath = std::filesystem::temp_directory_path(ec);
    if (ec) {
        api->sendReplyMessage(
            message->message(),
            access(res, Strings::FAILED_TO_DOWNLOAD_FILE));
        return;
    }
    params.srcPath = tmpPath / kDownloadFile.data();
    params.destPath = tmpPath / kOutputFile.data();
    std::string fileid;
    enum class MediaType { INVALID, MPEG4, WEBM, PNG } mediaType{};
    MessageAttrs attr{};

    auto replyMessage = message->reply();

    if (replyMessage->has<MessageAttrs::Photo>()) {
        fileid = replyMessage->get<MessageAttrs::Photo>()->fileId;
        mediaType = MediaType::PNG;
        attr = MessageAttrs::Photo;
    } else if (replyMessage->has<MessageAttrs::Sticker>()) {
        const auto stick = replyMessage->get<MessageAttrs::Sticker>();
        if (stick->isAnimated || stick->isVideo) {
#ifndef IMAGEPROC_HAVE_OPENCV
            api->sendReplyMessage(
                message->message(),
                access(res, Strings::CANNOT_ROTATE_NONSTATIC));
            return;
#endif
            mediaType = MediaType::WEBM;
        } else {
            mediaType = MediaType::PNG;
        }
        fileid = stick->fileId;
        attr = MessageAttrs::Sticker;
#ifdef IMAGEPROC_HAVE_OPENCV
    } else if (replyMessage->has<MessageAttrs::Animation>()) {
        fileid = replyMessage->get<MessageAttrs::Animation>()->fileId;
        mediaType = MediaType::MPEG4;
        attr = MessageAttrs::Animation;
    } else if (replyMessage->has<MessageAttrs::Video>()) {
        fileid = replyMessage->get<MessageAttrs::Video>()->fileId;
        mediaType = MediaType::MPEG4;
        attr = MessageAttrs::Video;
#endif
    } else {
        api->sendReplyMessage(message->message(),
                              access(res, Strings::REPLY_TO_A_MEDIA));
        return;
    }

    auto args = message->get<MessageAttrs::ParsedArgumentsList>();
    if (!try_parse(args[0], &rotation)) {
        api->sendReplyMessage(message->message(),
                              access(res, Strings::INVALID_ANGLE));
        return;
    }
    if (args.size() == 2) {
        params.options.greyscale = args[1] == "greyscale";
        params.options.invert_color = args[1] == "invert";
    }
    params.options.rotate_angle = rotation;

    // Process the image
    switch (mediaType) {
        case MediaType::WEBM: {
            params.srcPath.replace_extension(".webm");
            params.destPath.replace_extension(".webm");
            params.mimeType = "video/webm";
            params.target = PhotoBase::Target::kVideo;
        } break;
        case MediaType::MPEG4: {
            params.srcPath.replace_extension(".mp4");
            params.destPath.replace_extension(".mp4");
            params.mimeType = "video/mp4";
            params.target = PhotoBase::Target::kVideo;
        } break;
        case MediaType::PNG: {
            params.mimeType = "image/png";
            params.srcPath.replace_extension(".webp");
            params.destPath.replace_extension(".webp");
            params.target = PhotoBase::Target::kPhoto;
        } break;
        case MediaType::INVALID:
            return;
    }

    // Download the sticker file
    if (!api->downloadFile(params.srcPath, fileid)) {
        api->sendReplyMessage(message->message(),
                              access(res, Strings::FAILED_TO_DOWNLOAD_FILE));
        return;
    }

    if (auto ret = processFile(params); ret.ok()) {
        const auto infile = TgBot::InputFile::fromFile(params.destPath.string(),
                                                       params.mimeType);
        switch (attr) {
            case MessageAttrs::Photo:
                api->sendReplyPhoto(message->message(), infile,
                                    access(res, Strings::ROTATED_PICTURE));
                break;
            case MessageAttrs::Sticker:
                api->sendReplySticker(message->message(), infile);
                break;
            case MessageAttrs::Animation:
                api->sendReplyAnimation(message->message(), infile,
                                        access(res, Strings::ROTATED_PICTURE));
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
                              access(res, Strings::FAILED_TO_ROTATE_IMAGE));
        api->sendReplyMessage(message->message(), ret.message().data());
    }
    std::filesystem::remove(params.srcPath);  // Delete the temporary file
    std::filesystem::remove(params.destPath);
}
}  // namespace

extern "C" const struct DynModule DYN_COMMAND_EXPORT DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "rotatepic",
    .description = "Rotate a sticker/video/photo",
    .function = COMMAND_HANDLER_NAME(rotatepic),
    .valid_args = {
        .enabled = true,
        .counts = DynModule::craftArgCountMask<1,2>(),
        .split_type = DynModule::ValidArgs::Split::ByWhitespace,
        .usage = "/rotatepic angle [greyscale|invert]",
    },
};