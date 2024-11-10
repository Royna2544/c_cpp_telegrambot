#include <absl/status/status.h>

#include <TryParseStr.hpp>
#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>
#include <cctype>
#include <filesystem>
#include <imagep/ImageProcAll.hpp>
#include <string>
#include <string_view>

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
absl::Status processPhotoFile(ProcessImageParam& param) {
    ImageProcessingAll procAll(param.srcPath);
    if (procAll.read(param.target)) {
        LOG(INFO) << "Successfully read image";
        procAll.options = param.options;
        return procAll.processAndWrite(param.destPath);
    }
    return absl::InternalError("No backend suitable for reading");
}

constexpr std::string_view kDownloadFile = "inpic";
constexpr std::string_view kOutputFile = "outpic";

DECLARE_COMMAND_HANDLER(rotatepic) {
    int rotation = 0;
    bool greyscale = false;
    bool invert = false;
    std::optional<std::string> fileid;
    enum class MediaType { MPEG4, WEBM, PNG } mediaType{};

    if (message->replyMessage()->has<MessageAttrs::Photo>()) {
        fileid = message->replyMessage()->get<MessageAttrs::Photo>()->fileId;
        mediaType = MediaType::PNG;
    } else if (message->replyMessage()->has<MessageAttrs::Sticker>()) {
        const auto stick =
            message->replyMessage()->get<MessageAttrs::Sticker>();
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
#ifdef IMAGEPROC_HAVE_OPENCV
    } else if (message->replyMessage()->has<MessageAttrs::Animation>()) {
        fileid =
            message->replyMessage()->get<MessageAttrs::Animation>()->fileId;
        mediaType = MediaType::MPEG4;
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
        greyscale = args[1] == "greyscale";
        invert = args[1] == "invert";
    }

    // Process the image
    ProcessImageParam params{};
    params.srcPath = kDownloadFile.data();
    params.options.greyscale = greyscale;
    params.options.rotate_angle = rotation;
    params.options.invert_color = invert;  // Invert color
    params.destPath = kOutputFile.data();
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
    }

    // Download the sticker file
    if (!api->downloadFile(params.srcPath, fileid.value())) {
        api->sendReplyMessage(message->message(),
                              access(res, Strings::FAILED_TO_DOWNLOAD_FILE));
        return;
    }

    if (processPhotoFile(params).ok()) {
        const auto infile = TgBot::InputFile::fromFile(params.destPath.string(),
                                                       params.mimeType);
        if (message->replyMessage()->get<MessageAttrs::Sticker>()) {
            api->sendReplySticker(message->message(), infile);
        } else if (message->replyMessage()->get<MessageAttrs::Photo>()) {
            api->sendReplyPhoto(message->message(), infile,
                                access(res, Strings::ROTATED_PICTURE));
        } else if (message->replyMessage()->get<MessageAttrs::Animation>()) {
            api->sendReplyAnimation(message->message(), infile,
                                    access(res, Strings::ROTATED_PICTURE));
        }
    } else {
        api->sendReplyMessage(message->message(),
                              access(res, Strings::FAILED_TO_ROTATE_IMAGE));
    }
    std::filesystem::remove(params.srcPath);  // Delete the temporary file
    std::filesystem::remove(params.destPath);
}
}  // namespace

DYN_COMMAND_FN(n, module) {
    module.name = "rotatepic";
    module.description = "Rotate a sticker";
    module.flags = CommandModule::Flags::None;
    module.function = COMMAND_HANDLER_NAME(rotatepic);
    module.valid_arguments.enabled = true;
    module.valid_arguments.counts = {1, 2};
    module.valid_arguments.split_type =
        CommandModule::ValidArgs::Split::ByWhitespace;
    module.valid_arguments.usage = "/rotatepic angle [greyscale]";
    return true;
}
