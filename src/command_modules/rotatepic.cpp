#include <StringToolsExt.hpp>
#include <TgBotWrapper.hpp>
#include <TryParseStr.hpp>
#include <algorithm>
#include <boost/algorithm/string/split.hpp>
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
    MessageWrapper wrapper(tgWrapper, message);
    std::string extText = wrapper.getExtraText();
    std::vector<std::string> args;
    int rotation = 0;
    bool greyscale = false;

    if (extText.empty()) {
        wrapper.sendMessageOnExit("Usage: /rotatepic <angle> [greyscale]");
        return;
    }

    if (!wrapper.switchToReplyToMessage("Reply to a sticker or picture")) {
        return;
    }
    boost::split(args, extText, isspace);
    std::ranges::remove_if(args, isEmptyOrBlank);

    if (args.size() < 1 || args.size() > 2) {
        wrapper.sendMessageOnExit(
            "Invalid arguments. Use /rotatepic <angle> [greyscale]");
        return;
    }
    if (!try_parse(args[0], &rotation)) {
        wrapper.sendMessageOnExit("Invalid angle. (0-360 are allowed)");
        return;
    }
    if (args.size() == 2) {
        greyscale = args[1] == "greyscale";
    }
    std::optional<std::string> fileid;
    if (wrapper.hasSticker()) {
        const auto stick = wrapper.getSticker();
        if (stick->isAnimated || stick->isVideo) {
            wrapper.sendMessageOnExit(
                "Cannot rotate animated or video sticker");
        }
        fileid = stick->fileId;
    } else if (wrapper.hasPhoto()) {
        // Select the best quality photos available
        fileid = wrapper.getPhoto().back()->fileId;
    } else {
        wrapper.sendMessageOnExit("Reply to a sticker or photo");
    }

    if (!fileid) {
        return;
    }

    // Download the sticker file
    if (!tgWrapper->downloadFile(kDownloadFile.data(), fileid.value())) {
        wrapper.sendMessageOnExit("Failed to download sticker file.");
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
        const auto replyParams = std::make_shared<TgBot::ReplyParameters>();
        replyParams->messageId = message->messageId;
        replyParams->chatId = message->chat->id;
        if (wrapper.hasSticker()) {
            tgWrapper->sendReplySticker(message, infile);
        } else if (wrapper.hasPhoto()) {
            tgWrapper->sendReplyPhoto(message, infile, "Rotated picture");
        }
    } else {
        wrapper.sendMessageOnExit("Unknown image type, or processing failed");
    }
    std::filesystem::remove(params.srcPath);  // Delete the temporary file
    std::filesystem::remove(params.destPath);
}
}  // namespace

DYN_COMMAND_FN(n, module) {
    module.command = "rotatepic";
    module.description = "Rotate a sticker";
    module.flags = CommandModule::Flags::None;
    module.fn = COMMAND_HANDLER_NAME(rotatepic);
    return true;
}
