
#include <fmt/core.h>

#include <TgBotWrapper.hpp>
#include <algorithm>
#include <filesystem>
#include <functional>
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
    std::string emoji;
    int index;
    size_t maxIndex;

    void printProgress(const std::string& operation) const {
        DLOG(INFO) << fmt::format("{}: {} [{}/{}]", operation,
                                  filePath.string(), index, maxIndex);
    }
    auto operator<=>(const StickerData& other) const {
        return other.fileUniqueId <=> fileUniqueId;
    }
};

std::string ratio_to_percentage(double ratio) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2) << ratio * 100 << "%";
    return ss.str();
}

// Too many stickers in req to create a sticker set at once doesn't work
constexpr int GOOD_MAX_STICKERS_SIZE = 40;
// To not spam editMessage request
constexpr int RATELIMIT_DELIMITER_FOR_CONVERT_UPDATE = 5;

DECLARE_COMMAND_HANDLER(copystickers, api, message) {
    if (!message->replyToMessage_has<MessageExt::Attrs::Sticker>()) {
        api->sendReplyMessage(message, "Reply to a sticker");
        return;
    }
    const auto set = api->getStickerSet(
        message->replyToMessage_get<MessageExt::Attrs::Sticker>()->setName);
    if (!set) {
        api->sendReplyMessage(message, "Sticker set not found");
        return;
    }
    if (createDirectory("stickers")) {
        api->sendReplyMessage(message, "Failed to create stickers directory");
        return;
    }
    int counter = 0;
    if (std::ranges::any_of(set->stickers, [](const auto& sticker) {
            return sticker->isAnimated || sticker->isVideo;
        })) {
        LOG(WARNING) << "Sticker set contains animated stickers";
        api->sendReplyMessage(message, "Animated stickers are not supported");
        return;
    }

    LOG(INFO) << "Copy stickers from set " << std::quoted(set->title)
              << ". Started";

    const auto sentMessage =
        api->sendReplyMessage(message, "Starting conversion...");

    // Make a list of file IDs of all stickers in the set
    std::vector<StickerData> stickerData;
    std::vector<InputSticker::Ptr> stickerToSend;
    std::ranges::transform(
        set->stickers, std::back_inserter(stickerData),
        [&counter, set](const auto& sticker) {
            return StickerData{
                "stickers"_fs / (sticker->fileUniqueId + ".webp"),
                sticker->fileUniqueId,
                sticker->fileId,
                sticker->emoji,
                counter++,
                std::min<size_t>(set->stickers.size(), GOOD_MAX_STICKERS_SIZE)};
        });

    // Remove duplicates by unique file IDs
    std::sort(stickerData.begin(), stickerData.end());
    stickerData.erase(
        std::ranges::unique(stickerData, std::greater<>()).begin(),
        stickerData.end());

    // Download all stickers from the set
    LOG(INFO) << fmt::format("Now downloading {} stickers", stickerData.size());
    size_t count = 0;
    for (const auto& sticker : stickerData) {
        if (count % RATELIMIT_DELIMITER_FOR_CONVERT_UPDATE == 0) {
            const auto percent =
                ratio_to_percentage(static_cast<double>(count) /
                                    static_cast<double>(stickerData.size()));
            api->editMessage(sentMessage,
                             fmt::format("Converting stickers... {}", percent));
        }
        sticker.printProgress("Downloading");
        if (!api->downloadFile(sticker.filePath, sticker.fileId)) {
            api->sendReplyMessage(message, "Failed to download sticker file");
            return;
        }
        sticker.printProgress("Converting");
        ImageProcessingAll imageProc(sticker.filePath);
        if (!imageProc.read()) {
            api->sendReplyMessage(message, "Failed to read sticker file");
            return;
        }
        imageProc.to_greyscale();
        if (!imageProc.write(sticker.filePath)) {
            api->sendReplyMessage(message, "Failed to write sticker file");
            return;
        }
        sticker.printProgress("Uploading");
        const auto file = api->uploadStickerFile(
            message->from->id,
            InputFile::fromFile(sticker.filePath.generic_string(),
                                "image/webp"),
            "static");
        if (!file) {
            api->sendReplyMessage(message, "Failed to upload sticker file");
            return;
        }
        sticker.printProgress("Sending");
        auto inputSticker = std::make_shared<InputSticker>();
        inputSticker->sticker = file->fileId;
        inputSticker->format = "static";
        inputSticker->emojiList.emplace_back(sticker.emoji);
        stickerToSend.emplace_back(inputSticker);
        std::filesystem::remove(sticker.filePath);
        ++count;
    }

    // Create a new sticker set with the same name and title as the
    // original set, but with all stickers replaced with the converted
    // ones and with a custom emoji
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
        api->createNewStickerSet(message->from->id, setName, title,
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
    module.function = COMMAND_HANDLER_NAME(copystickers);
    return true;
}
