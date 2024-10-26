#include <StringToolsExt.hpp>
#include <TryParseStr.hpp>
#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>
#include <cctype>
#include <filesystem>
#include <imagep/ImageProcAll.hpp>
#include <memory>
#include <string>
#include <string_view>

#include "StringResLoader.hpp"
#include "api/MessageExt.hpp"

struct ProcessImageParam {
    std::filesystem::path srcPath;
    std::filesystem::path destPath;
    int rotation;
    bool greyscale;
};

namespace {
bool processPhotoFile(ProcessImageParam& param) {
    ImageProcessingAll procAll(param.srcPath);
    if (procAll.read()) {
        LOG(INFO) << "Successfully read image";
        const auto res = procAll.rotate(param.rotation);
        if (!res.ok()) {
            LOG(ERROR) << "Failed to rotate image: " << res;
            return false;
        }
        if (param.greyscale) {
            procAll.to_greyscale();
        }
        return procAll.write(param.destPath);
    }
    return false;
}

constexpr std::string_view kDownloadFile = "inpic.bin";
constexpr std::string_view kOutputFile = "outpic.png";

DECLARE_COMMAND_HANDLER(rotatepic) {
    std::vector<std::string> args =
        message->get<MessageAttrs::ParsedArgumentsList>();
    int rotation = 0;
    bool greyscale = false;
    std::optional<std::string> fileid;

    if (message->replyMessage()->has<MessageAttrs::Photo>()) {
        fileid = message->replyMessage()->get<MessageAttrs::Photo>()->fileId;
    } else if (message->replyMessage()->has<MessageAttrs::Sticker>()) {
        const auto stick =
            message->replyMessage()->get<MessageAttrs::Sticker>();
        if (stick->isAnimated || stick->isVideo) {
            api->sendReplyMessage(
                message->message(),
                access(res, Strings::CANNOT_ROTATE_NONSTATIC));
            return;
        }
        fileid = stick->fileId;
    }

    if (!try_parse(args[0], &rotation)) {
        api->sendReplyMessage(message->message(),
                              access(res, Strings::INVALID_ANGLE));
        return;
    }
    if (args.size() == 2) {
        greyscale = args[1] == "greyscale";
    }

    // Download the sticker file
    if (!api->downloadFile(kDownloadFile.data(), fileid.value())) {
        api->sendReplyMessage(message->message(),
                              access(res, Strings::FAILED_TO_DOWNLOAD_FILE));
        return;
    }

    // Round it under 360
    rotation = rotation % PhotoBase::kAngleMax;

    // Process the image
    ProcessImageParam params{};
    params.srcPath = kDownloadFile.data();
    params.greyscale = greyscale;
    params.rotation = rotation;
    params.destPath = kOutputFile.data();

    if (processPhotoFile(params)) {
        const auto infile =
            TgBot::InputFile::fromFile(params.destPath.string(), "image/png");
        if (message->replyMessage()->get<MessageAttrs::Sticker>()) {
            api->sendReplySticker(message->message(), infile);
        } else if (message->replyMessage()->get<MessageAttrs::Photo>()) {
            api->sendReplyPhoto(message->message(), infile,
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
