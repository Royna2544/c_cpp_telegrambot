#include <BotReplyMessage.h>

#include <MessageWrapper.hpp>
#include <StringToolsExt.hpp>
#include <TryParseStr.hpp>
#include <algorithm>
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
        LOG(INFO) << "Successfully read image";
        LOG(INFO) << "Rotating image by " << param.rotation << " degrees";
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
                LOG(INFO) << "Successfully rotated image";
                break;
        }
        if (param.greyscale) {
            LOG(INFO) << "Converting image to greyscale";
            inst.to_greyscale();
        }
        LOG(INFO) << "Writing image to " << param.destPath;
        return inst.write(param.destPath);
    } else {
        LOG(ERROR) << "Counld't read or parse image";
    }
    return false;
}

bool processPhotoFile(ProcessImageParam& param) {
    ImageVariants images;

    // First try: PNG
    LOG(INFO) << "First try: PNG";
    images.emplace<PngImage>();
    param.destPath = "output.png";
    if (tryToProcess<0>(images, param)) {
        LOG(INFO) << "PNG liked it. Wrote image: " << param.destPath;
        return true;
    }

    // Second try: WebP
    LOG(INFO) << "Second try: WebP";
    images.emplace<WebPImage>();
    param.destPath = "output.webp";
    if (tryToProcess<1>(images, param)) {
        LOG(INFO) << "WebP liked it. Wrote image: " << param.destPath;
        return true;
    }

    // Third try: JPEG
    LOG(INFO) << "Third try: JPEG";
    images.emplace<JPEGImage>();
    param.destPath = "output.jpg";
    if (tryToProcess<2>(images, param)) {
        LOG(INFO) << "JPEG liked it. Wrote image: " << param.destPath;
        return true;
    }

    // Fourth try: OpenCV
    LOG(INFO) << "Fourth try: OpenCV";
    images.emplace<OpenCVImage>();
    param.destPath = "output.png";
    if (tryToProcess<3>(images, param)) {
        LOG(INFO) << "OpenCV liked it. Wrote image: " << param.destPath;
        return true;
    }

    // Failed to process
    LOG(ERROR) << "No one liked it. Failed to process";
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

    // Round it under 360
    rotation = rotation % PhotoBase::kAngleMax;

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
