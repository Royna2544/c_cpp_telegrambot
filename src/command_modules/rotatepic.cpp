#include <MessageWrapper.hpp>
#include <boost/algorithm/string/split.hpp>
#include <cctype>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <libPNG.hpp>
#include <libWEBP.hpp>
#include <string>
#include <string_view>
#include <variant>

#include "BotReplyMessage.h"
#include "CommandModule.h"
#include "TryParseStr.hpp"
#include "tgbot/types/InputFile.h"

struct ProcessImageParam {
    std::filesystem::path srcPath;
    std::filesystem::path destPath;
    int rotation;
    bool greyscale;
};

namespace {

template <int index>
bool tryToProcess(std::variant<PngImage, WebPImage>& images,
                  ProcessImageParam param) {
    auto& inst = std::get<index>(images);
    if (inst.read(param.srcPath)) {
        switch (param.rotation) {
            case 0:
                break;
            case 90:
                inst.rotate_image_90();
                break;
            case 180:
                inst.rotate_image_180();
                break;
            case 270:
                inst.rotate_image_270();
                break;
            default:
                LOG(WARNING) << "Invalid rotation angle: " << param.rotation;
                break;
        };
        if (param.greyscale) {
            inst.to_greyscale();
        }
        return inst.write(param.destPath);
    }
    return false;
}

bool processPhotoFile(ProcessImageParam& param) {
    std::variant<PngImage, WebPImage> images;
    LOG(INFO) << "Processing image " << param.srcPath << "...";

    // First try: PNG
    LOG(INFO) << "First try: PNG";
    images.emplace<PngImage>();
    param.destPath = "output.png";
    if (tryToProcess<0>(images, param)) {
        LOG(INFO) << "Wrote image: " << param.destPath;
        return true;
    }

    std::variant<PngImage, WebPImage> imagesTry2;
    // Second try: WebP
    LOG(INFO) << "Second try: WebP";
    images.emplace<WebPImage>();
    param.destPath = "output.webp";
    if (tryToProcess<1>(images, param)) {
        LOG(INFO) << "Wrote image: " << param.destPath;
        return true;
    }

    LOG(ERROR) << "Failed to process";
    return false;
}

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

    if (!wrapper.switchToReplyToMessage("Reply to a sticker")) {
        return;
    }
    boost::split(args, extText, isspace);
    if (args.size() < 1 || args.size() > 2) {
        wrapper.sendMessageOnExit("Invalid arguments. Use /rotatepic <angle> [greyscale]");
        return;
    }
    if (!try_parse(args[0], &rotation)) {
        wrapper.sendMessageOnExit("Invalid angle. Use one of 90/180/270");
        return;
    }
    if (args.size() == 2) {
        greyscale = args[1] == "greyscale";
    }

    if (wrapper.hasSticker()) {
        const auto sticker = wrapper.getSticker();
        const auto file = bot.getApi().getFile(sticker->fileId);
        constexpr std::string_view kDownloadFile = "tmp.png";
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

        // Process the image
        ProcessImageParam params{};
        params.srcPath = kDownloadFile.data();
        params.greyscale = greyscale;
        params.rotation = rotation;

        if (processPhotoFile(params)) {
            bot.getApi().sendSticker(
                wrapper.getChatId(),
                TgBot::InputFile::fromFile(params.destPath.string(), "image/png"));
        } else {
            wrapper.sendMessageOnExit("Unknown image type, or processing failed");
        }
    } else {
        wrapper.sendMessageOnExit("Reply to a sticker please.");
    }
}
}  // namespace

void loadcmd_rotatepic(CommandModule& module) {
    module.command = "rotatepic";
    module.description = "Rotate a sticker";
    module.flags = CommandModule::Flags::None;
    module.fn = rotateStickerCommand;
}
