
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

// Too many stickers in req to create a sticker set at once doesn't work
constexpr int GOOD_KNOWN_MAX_STICKER_CREATE_REQUESTS = 60;

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
            return sticker->isAnimated || sticker->isVideo;
        })) {
        LOG(WARNING) << "Sticker set contains animated stickers";
        wrapper.sendMessageOnExit("Animated stickers are not supported");
        return;
    }

    LOG(INFO) << "Copy stickers from set " << set->title << ". Started";

    const auto sentMessage =
        api->sendMessage(message, "Starting conversion...");

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
    
    if (stickerData.size() > GOOD_KNOWN_MAX_STICKER_CREATE_REQUESTS) {
        // Limit the number of requests to avoid hitting the API rate limit
        LOG(INFO) << "Limiting stickers size to" << GOOD_KNOWN_MAX_STICKER_CREATE_REQUESTS;
        stickerData.resize(GOOD_KNOWN_MAX_STICKER_CREATE_REQUESTS);
    }

    // Download all stickers from the set
    LOG(INFO) << "Now downloading " << set->stickers.size() << " stickers";
    int count = 0;
    for (const auto& sticker : stickerData) {
        if (count % 10 == 0) {
            api->editMessage(sentMessage,
                             "Converting stickers... " + std::to_string(count) +
                                 "/" + std::to_string(stickerData.size()));
        }
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
            InputFile::fromFile(sticker.filePath.generic_string(),
                                "image/webp"),
            "static");
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
        ++count;
    }
    // Create a new sticker set with the same name and title as the original
    // set, but with all stickers replaced with the converted ones and with a
    // custom emoji
    std::string setName = "grey_" + set->name;
    const std::string title = "Greyed-out " + set->title;
    {
        const static std::regex regex(R"(_by_\w+$)");
        std::smatch match;
        if (std::regex_search(setName, match, regex)) {
            // Trim the author out
            setName =
                setName.substr(0, match.position()) + match.suffix().str();
        }
        setName += "_by_" + api->getBotUser()->username;
    }

    api->editMessage(sentMessage, "Creating new sticker set...");
    LOG(INFO) << "Now creating new sticker set, name: " << std::quoted(setName)
              << ", title: " << std::quoted(title)
              << " size: " << stickerToSend.size();
    try {
        api->createNewStickerSet(wrapper.getUser()->id, setName, title,
                                 stickerToSend, Sticker::Type::Regular);
        LOG(INFO) << "Created: https://t.me/addstickers/" + setName;
        api->editMessage(
            sentMessage,
            "Sticker pack created: https://t.me/addstickers/" + setName);
    } catch (const TgBot::TgException& e) {
        DLOG(ERROR) << "Failed to create new sticker set: " << e.what();
        api->editMessage(sentMessage,
                         "Failed to create new sticker set: "s + e.what());

    }
}

DYN_COMMAND_FN(/*name*/, module) {
    module.command = "copystickers";
    module.description = "Copy sticker pack with a remap";
    module.flags = CommandModule::Flags::None;
    module.fn = COMMAND_HANDLER_NAME(copystickers);
    return true;
}
