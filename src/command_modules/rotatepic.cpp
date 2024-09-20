#include <StringToolsExt.hpp>
#include <TgBotWrapper.hpp>
#include <TryParseStr.hpp>
#include <cctype>
#include <filesystem>
#include <imagep/ImageProcAll.hpp>
#include <memory>
#include <string>
#include <string_view>

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

DECLARE_COMMAND_HANDLER(rotatepic, tgWrapper, message) {
    std::vector<std::string> args = message->arguments();
    int rotation = 0;
    bool greyscale = false;
    std::optional<std::string> fileid;

    if (message->replyToMessage_has<MessageExt::Attrs::Photo>()) {
        const auto photo = message->replyToMessage->photo;
        // Select the best quality photos available
        fileid = photo.back()->fileId;
    } else if (message->replyToMessage_has<MessageExt::Attrs::Sticker>()) {
        const auto stick = message->replyToMessage->sticker;
        if (stick->isAnimated || stick->isVideo) {
            tgWrapper->sendReplyMessage(
                message, "Cannot rotate animated or video sticker");
        }
        fileid = stick->fileId;
    }

    if (!try_parse(args[0], &rotation)) {
        tgWrapper->sendReplyMessage(message,
                                    "Invalid angle. (0-360 are allowed)");
        return;
    }
    if (args.size() == 2) {
        greyscale = args[1] == "greyscale";
    }

    // Download the sticker file
    if (!tgWrapper->downloadFile(kDownloadFile.data(), fileid.value())) {
        tgWrapper->sendReplyMessage(message,
                                    "Failed to download sticker file.");
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
        if (message->replyToMessage_has<MessageExt::Attrs::Sticker>()) {
            tgWrapper->sendReplySticker(message, infile);
        } else if (message->replyToMessage_has<MessageExt::Attrs::Photo>()) {
            tgWrapper->sendReplyPhoto(message, infile, "Rotated picture");
        }
    } else {
        tgWrapper->sendReplyMessage(message,
                                    "Unknown image type, or processing failed");
    }
    std::filesystem::remove(params.srcPath);  // Delete the temporary file
    std::filesystem::remove(params.destPath);
}
}  // namespace

DYN_COMMAND_FN(n, module) {
    module.command = "rotatepic";
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
