
#include <TgBotWrapper.hpp>
#include <algorithm>
#include <filesystem>
#include <imagep/ImageProcAll.hpp>
#include <libos/libfs.hpp>
#include <memory>
#include <regex>

using std::string_literals::operator""s;

std::filesystem::path operator""_fs(const char* str, const size_t /*size*/) {
    return str;
}

struct StickerData {
    std::filesystem::path filePath;
    std::string fileUniqueId;
    std::string fileId;
    int index;
    size_t maxIndex;

    void printProgress(const std::string& operation) const {
        DLOG(INFO) << operation << ": " << filePath << " [" << index << "/"
                   << maxIndex << "]";
    }
};

DECLARE_COMMAND_HANDLER(copystickers, api, message) {
    MessageWrapper wrapper(api, message);
    if (!wrapper.switchToReplyToMessage("Reply to a sticker")) {
        return;
    }
    if (!wrapper.hasSticker()) {
        wrapper.sendMessageOnExit("Please send a sticker");
        return;
    }
    const auto set = api->getStickerSet(wrapper.getSticker()->setName);
    if (!set) {
        wrapper.sendMessageOnExit("Sticker set not found");
        return;
    }
    if (createDirectory("stickers")) {
        wrapper.sendMessageOnExit("Failed to create stickers directory");
        return;
    }
    int counter = 0;
    if (std::ranges::any_of(set->stickers, [](const auto& sticker) {
            return sticker->type != Sticker::Type::Regular;
        })) {
        LOG(WARNING) << "Sticker set contains animated stickers";
        wrapper.sendMessageOnExit("Animated stickers are not supported");
        return;
    }

    // Make a list of file IDs of all stickers in the set
    std::vector<StickerData> stickerData;
    std::vector<InputSticker::Ptr> stickerToSend;
    std::ranges::transform(
        set->stickers, std::back_inserter(stickerData),
        [&counter, set](const auto& sticker) {
            return StickerData{
                "stickers"_fs / (sticker->fileUniqueId + ".webp"),
                sticker->fileUniqueId, sticker->fileId, counter++,
                set->stickers.size()};
        });

    // Download all stickers from the set
    DLOG(INFO) << "Now downloading " << set->stickers.size() << " stickers";
    for (const auto& sticker : stickerData) {
        sticker.printProgress("Downloading");
        if (!api->downloadFile(sticker.filePath, sticker.fileId)) {
            wrapper.sendMessageOnExit("Failed to download sticker file");
            return;
        }
        sticker.printProgress("Converting");
        ImageProcessingAll imageProc(sticker.filePath);
        if (!imageProc.read()) {
            wrapper.sendMessageOnExit("Failed to read sticker file");
            return;
        }
        imageProc.to_greyscale();
        if (!imageProc.write(sticker.filePath)) {
            wrapper.sendMessageOnExit("Failed to write sticker file");
            return;
        }
        sticker.printProgress("Uploading");
        const auto file = api->uploadStickerFile(
            wrapper.getUser()->id,
            InputFile::fromFile(sticker.filePath, "image/webp"), "static");
        if (!file) {
            wrapper.sendMessageOnExit("Failed to upload sticker file");
            return;
        }
        sticker.printProgress("Sending");
        auto inputSticker = std::make_shared<InputSticker>();
        inputSticker->sticker = file->fileId;
        inputSticker->format = "static";
        inputSticker->emojiList.emplace_back("😠");
        stickerToSend.emplace_back(inputSticker);
        std::filesystem::remove(sticker.filePath);
    }
    // Create a new sticker set with the same name and title as the original
    // set, but with all stickers replaced with the converted ones and with a
    // custom emoji
    std::string setName = "copy_of_" + set->name;
    const std::string title = "Copy of " + set->title;
    {
        const static std::regex regex(R"(_by_\w+$)");
        std::smatch match;
        if (std::regex_search(set->title, match, regex)) {
            // Trim the author out
            setName = setName.substr(0, match.position()) + match.suffix().str();
        }
        setName += "_by_" + api->getBotUser()->username;
    }
    DLOG(INFO) << "Now creating new sticker set, name: " << std::quoted(setName)
               << ", title: " << std::quoted(title)
               << " size: " << stickerToSend.size();
    try {
        api->createNewStickerSet(wrapper.getUser()->id, setName, title,
                                 stickerToSend, Sticker::Type::Regular);
        LOG(INFO) << "Created: https://t.me/addstickers/" + setName;
    } catch (const TgBot::TgException& e) {
        DLOG(ERROR) << "Failed to create new sticker set: " << e.what();
        wrapper.sendMessageOnExit("Failed to create new sticker set: "s +
                                  e.what());
        return;
    }

    wrapper.sendMessageOnExit("Sticker pack creation completed successfully");
}

DYN_COMMAND_FN(/*name*/, module) {
    module.command = "copystickers";
    module.description = "Copy sticker pack with a remap";
    module.flags = CommandModule::Flags::None;
    module.fn = COMMAND_HANDLER_NAME(copystickers);
    return true;
}
