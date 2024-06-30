#include <BotReplyMessage.h>

#include <MessageWrapper.hpp>
#include <TryParseStr.hpp>
#include <boost/algorithm/string/split.hpp>
#include <cctype>
#include <database/bot/TgBotDatabaseImpl.hpp>
#include <filesystem>
#include <imagep/ImageProcessingAll.hpp>
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

using ImageVariants = std::variant<PngImage, WebPImage, JPEGImage, OpenCVImage>;
template <int index>
bool tryToProcess(ImageVariants& images, ProcessImageParam param) {
    auto& inst = std::get<index>(images);
    if (inst.read(param.srcPath)) {
        switch (inst.rotate_image(param.rotation)) {
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
                break;
        }
        if (param.greyscale) {
            inst.to_greyscale();
        }
        return inst.write(param.destPath);
    }
    return false;
}

bool processPhotoFile(ProcessImageParam& param) {
    ImageVariants images;
    LOG(INFO) << "Processing image " << param.srcPath << "...";

    // First try: PNG
    LOG(INFO) << "First try: PNG";
    images.emplace<PngImage>();
    param.destPath = "output.png";
    if (tryToProcess<0>(images, param)) {
        LOG(INFO) << "Wrote image: " << param.destPath;
        return true;
    }

    // Second try: WebP
    LOG(INFO) << "Second try: WebP";
    images.emplace<WebPImage>();
    param.destPath = "output.webp";
    if (tryToProcess<1>(images, param)) {
        LOG(INFO) << "Wrote image: " << param.destPath;
        return true;
    }

    // Third try: JPEG
    LOG(INFO) << "Third try: JPEG";
    images.emplace<JPEGImage>();
    param.destPath = "output.jpg";
    if (tryToProcess<2>(images, param)) {
        LOG(INFO) << "Wrote image: " << param.destPath;
        return true;
    }

    // Fourth try: OpenCV
    LOG(INFO) << "Fourth try: OpenCV";
    images.emplace<OpenCVImage>();
    param.destPath = "output.png";
    if (tryToProcess<3>(images, param)) {
        LOG(INFO) << "Wrote image: " << param.destPath;
        return true;
    }

    // Failed to process
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
        wrapper.sendMessageOnExit(
            "Invalid arguments. Use /rotatepic <angle> [greyscale]");
        return;
    }
    if (!try_parse(args[0], &rotation)) {
        wrapper.sendMessageOnExit("Invalid angle. Use one of 90/180/270");
        return;
    }
    if (args.size() == 2) {
        greyscale = args[1] == "greyscale";
    }
    std::optional<std::string> fileid;
    if (wrapper.hasSticker()) {
        fileid = wrapper.getSticker()->fileId;
    } else if (wrapper.hasPhoto()) {
        fileid = wrapper.getPhoto().back()->fileId;
    } else {
        wrapper.sendMessageOnExit("Reply to a sticker or photo");
        return;
    }

    const auto file = bot.getApi().getFile(fileid.value());
    constexpr std::string_view kDownloadFile = "inpic.bin";
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
        const auto infile =
            TgBot::InputFile::fromFile(params.destPath.string(), "image/png");
        const auto replyParams = std::make_shared<TgBot::ReplyParameters>();
        replyParams->messageId = message->messageId;
        replyParams->chatId = message->chat->id;
        if (wrapper.hasSticker()) {
            bot.getApi().sendSticker(wrapper.getChatId(), infile, replyParams);
        } else if (wrapper.hasPhoto()) {
            bot.getApi().sendPhoto(wrapper.getChatId(), infile, "Rotated picture", replyParams);
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
