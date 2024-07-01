#include <BotReplyMessage.h>

#include <MessageWrapper.hpp>
#include <StringToolsExt.hpp>
#include <TryParseStr.hpp>
#include <algorithm>
#include <boost/algorithm/string/split.hpp>
#include <cctype>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <filesystem>
#include <imagep/ImageProcAll.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <variant>

#include "CommandModule.h"

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
        switch (procAll.rotate(param.rotation)) {
            case PhotoBase::Result::kErrorInvalidArgument:
                LOG(ERROR) << "Invalid rotation angle";
                return false;
            case PhotoBase::Result::kErrorUnsupportedAngle:
                LOG(ERROR) << "Unsupported rotation angle";
                return false;
            case PhotoBase::Result::kErrorNoData:
                LOG(ERROR) << "No data available to rotate (internal error)";
                return false;
            case PhotoBase::Result::kSuccess:
                LOG(INFO) << "Successfully rotated image";
                break;
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

void rotateStickerCommand(const Bot& bot, const Message::Ptr message) {
    MessageWrapper wrapper(bot, message);
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

    const auto file = bot.getApi().getFile(fileid.value());
    if (!file) {
        wrapper.sendMessageOnExit("Failed to download sticker file.");
        return;
    }
    // Download the sticker
    std::string buffer = bot.getApi().downloadFile(file->filePath);
    // Save the sticker to a temporary file
    std::ofstream ofs(kDownloadFile.data());
    ofs.write(buffer.data(), buffer.size());
    ofs.close();

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
            bot.getApi().sendSticker(wrapper.getChatId(), infile, replyParams);
        } else if (wrapper.hasPhoto()) {
            bot.getApi().sendPhoto(wrapper.getChatId(), infile,
                                   "Rotated picture", replyParams);
        }
    } else {
        wrapper.sendMessageOnExit("Unknown image type, or processing failed");
    }
    std::filesystem::remove(params.srcPath);  // Delete the temporary file
    std::filesystem::remove(params.destPath);
}
}  // namespace

void loadcmd_rotatepic(CommandModule& module) {
    module.command = "rotatepic";
    module.description = "Rotate a sticker";
    module.flags = CommandModule::Flags::None;
    module.fn = rotateStickerCommand;
}
