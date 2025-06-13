#include <fmt/format.h>
#include <tgbot/TgException.h>

#include <algorithm>
#include <api/CommandModule.hpp>
#include <api/MessageExt.hpp>
#include <api/StringResLoader.hpp>
#include <api/TgBotApi.hpp>
#include <filesystem>
#include <functional>
#include <imagep/ImageProcAll.hpp>
#include <libfs.hpp>
#include <regex>

#include "ImagePBase.hpp"

using std::string_view_literals::operator""sv;

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
    return fmt::format("{:.2f}%", ratio);
}

// Too many stickers in req to create a sticker set at once doesn't work
constexpr int GOOD_MAX_STICKERS_SIZE = 40;
// To not spam editMessage request
constexpr int RATELIMIT_DELIMITER_FOR_CONVERT_UPDATE = 5;

DECLARE_COMMAND_HANDLER(copystickers) {
    if (!message->reply()->has<MessageAttrs::Sticker>()) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::REPLY_TO_A_STICKER));
        return;
    }
    const auto sticker = message->reply()->get<MessageAttrs::Sticker>();
    if (!sticker->setName) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::STICKER_SET_NOT_FOUND));
        return;
    }
    const auto set = api->getStickerSet(*sticker->setName);
    if (!set) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::STICKER_SET_NOT_FOUND));
        return;
    }
    if (noex_fs::create_directories("stickers"_fs)) {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::FAILED_TO_CREATE_DIRECTORY));
        return;
    }
    int counter = 0;
    if (std::ranges::any_of(set->stickers, [](const auto& sticker) {
            return sticker->isAnimated || sticker->isVideo;
        })) {
        api->sendReplyMessage(
            message->message(),
            res->get(Strings::ANIMATED_STICKERS_NOT_SUPPORTED));
        return;
    }

    LOG(INFO) << "Copy stickers from set " << std::quoted(set->title)
              << ". Started";

    const auto sentMessage = api->sendReplyMessage(
        message->message(), res->get(Strings::STARTING_CONVERSION));

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
                sticker->emoji.value_or("👍"),
                counter++,
                std::min<size_t>(set->stickers.size(), GOOD_MAX_STICKERS_SIZE)};
        });

    // Remove duplicates by unique file IDs
    std::ranges::sort(stickerData, std::greater<>());
    auto [s, e] = std::ranges::unique(stickerData, std::greater<>());
    stickerData.erase(s, e);

    // Download all stickers from the set
    LOG(INFO) << fmt::format("Now downloading {} stickers", stickerData.size());
    size_t count = 0;
    for (const auto& sticker : stickerData) {
        if (count % RATELIMIT_DELIMITER_FOR_CONVERT_UPDATE == 0) {
            const auto ratio = static_cast<double>(count) /
                               static_cast<double>(stickerData.size());
            api->editMessage(
                sentMessage,
                fmt::format("{}... {:.2f}%",
                            res->get(Strings::CONVERSION_IN_PROGRESS), ratio));
        }
        sticker.printProgress("Downloading");
        if (!api->downloadFile(sticker.filePath, sticker.fileId)) {
            api->sendReplyMessage(message->message(),
                                  res->get(Strings::FAILED_TO_DOWNLOAD_FILE));
            return;
        }
        sticker.printProgress("Converting");
        ImageProcessingAll imageProc(sticker.filePath);
        if (!imageProc.read(PhotoBase::Target::kPhoto)) {
            api->sendReplyMessage(message->message(),
                                  res->get(Strings::FAILED_TO_READ_FILE));
            return;
        }
        imageProc.options.greyscale = true;
        auto ret = imageProc.processAndWrite(sticker.filePath);
        if (!ret.ok()) {
            api->sendReplyMessage(message->message(),
                                  res->get(Strings::FAILED_TO_WRITE_FILE));
            return;
        }
        sticker.printProgress("Uploading");
        const auto file = api->uploadStickerFile(
            message->get<MessageAttrs::User>()->id,
            InputFile::fromFile(sticker.filePath.generic_string(),
                                "image/webp"),
            TgBot::Api::StickerFormat::Static);
        if (!file) {
            api->sendReplyMessage(message->message(),
                                  res->get(Strings::FAILED_TO_UPLOAD_FILE));
            return;
        }
        sticker.printProgress("Sending");
        auto inputSticker = std::make_shared<InputSticker>();
        inputSticker->sticker = file->fileId;
        inputSticker->format = "static";
        inputSticker->emojiList.emplace_back(sticker.emoji);
        stickerToSend.emplace_back(std::move(inputSticker));
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
        setName += "_by_" + api->getBotUser()->username.value();
    }

    api->editMessage(
        sentMessage,
        fmt::format("{}...", res->get(Strings::CREATING_NEW_STICKER_SET)));
    LOG(INFO) << "Now creating new sticker set, name: " << std::quoted(setName)
              << ", title: " << std::quoted(title)
              << " size: " << stickerToSend.size();
    try {
        api->createNewStickerSet(message->get<MessageAttrs::User>()->id,
                                 setName, title, stickerToSend,
                                 Sticker::Type::Regular);
        LOG(INFO) << "Created: https://t.me/addstickers/" + setName;
        api->editMessage(
            sentMessage,
            fmt::format("{}: https://t.me/addstickers/{}",
                        res->get(Strings::STICKER_PACK_CREATED), setName));
    } catch (const TgBot::TgException& e) {
        LOG(ERROR) << "Failed to create new sticker set: " << e.what();
        api->editMessage(
            sentMessage,
            fmt::format("{}: {}",
                        res->get(Strings::FAILED_TO_CREATE_NEW_STICKER_SET),
                        e.what()));
    }
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "copystickers",
    .description = "Copy sticker pack with a remap",
    .function = COMMAND_HANDLER_NAME(copystickers),
    .valid_args = {},
};
