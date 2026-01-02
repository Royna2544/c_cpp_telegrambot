#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Media.hpp"
#include "forwards.hpp"

namespace tgbot_api {

/**
 * @brief Parse modes for message formatting
 */
enum class ParseMode {
    None,
    Markdown,
    MarkdownV2,
    HTML
};

/**
 * @brief Sticker formats
 */
enum class StickerFormat {
    Static,
    Animated,
    Video
};

/**
 * @brief This object represents the contents of a file to be uploaded.
 */
class InputFile {
   public:
    using Ptr = InputFile*;

    std::string data;
    std::string mimeType;
    std::string fileName;

    /**
     * @brief Creates an InputFile from file path
     */
    static Ptr fromFile(const std::string& filePath,
                        const std::string& mimeType = "");
};

/**
 * @brief This object describes a sticker to be added to a sticker set.
 */
class InputSticker {
   public:
    using Ptr = InputSticker*;

    InputFile::Ptr sticker;
    std::string format;
    std::vector<std::string> emojiList;
};

/**
 * @brief This object represents a sticker set.
 */
class StickerSet {
   public:
    using Ptr = StickerSet*;

    std::string name;
    std::string title;
    Sticker::Type stickerType{Sticker::Type::Regular};
    std::vector<Sticker::Ptr> stickers;
};

/**
 * @brief Base class for reply markup types.
 */
class GenericReply {
   public:
    using Ptr = GenericReply*;
    virtual ~GenericReply() = default;
};

/**
 * @brief This object represents an inline keyboard button.
 */
class InlineKeyboardButton {
   public:
    using Ptr = InlineKeyboardButton*;

    std::string text;
    std::optional<std::string> url;
    std::optional<std::string> callbackData;
};

/**
 * @brief This object represents an inline keyboard that appears right next to
 * the message.
 */
class InlineKeyboardMarkup : public GenericReply {
   public:
    using Ptr = InlineKeyboardMarkup*;

    std::vector<std::vector<InlineKeyboardButton::Ptr>> inlineKeyboard;
};

/**
 * @brief This object represents a custom keyboard with reply options.
 */
class KeyboardButton {
   public:
    using Ptr = KeyboardButton*;

    std::string text;
    std::optional<bool> requestContact;
    std::optional<bool> requestLocation;
};

/**
 * @brief This object represents a custom keyboard.
 */
class ReplyKeyboardMarkup : public GenericReply {
   public:
    using Ptr = ReplyKeyboardMarkup*;

    std::vector<std::vector<KeyboardButton::Ptr>> keyboard;
    std::optional<bool> resizeKeyboard;
    std::optional<bool> oneTimeKeyboard;
    std::optional<bool> selective;
};

/**
 * @brief Upon receiving a message with this object, Telegram clients will
 * remove the current custom keyboard.
 */
class ReplyKeyboardRemove : public GenericReply {
   public:
    using Ptr = ReplyKeyboardRemove*;

    bool removeKeyboard{true};
    std::optional<bool> selective;
};

/**
 * @brief Upon receiving a message with this object, Telegram clients will
 * display a reply interface.
 */
class ForceReply : public GenericReply {
   public:
    using Ptr = ForceReply*;

    bool forceReply{true};
    std::optional<std::string> inputFieldPlaceholder;
    std::optional<bool> selective;
};

/**
 * @brief This object describes the bot's menu button in a private chat.
 */
class ReplyParameters {
   public:
    using Ptr = ReplyParameters*;

    MessageId messageId{};
    std::optional<MessageThreadId> messageThreadId;
    std::optional<bool> allowSendingWithoutReply;
};

/**
 * @brief Describes the current status of a chat member's permissions.
 */
class ChatPermissions {
   public:
    using Ptr = ChatPermissions*;

    std::optional<bool> canSendMessages;
    std::optional<bool> canSendAudios;
    std::optional<bool> canSendDocuments;
    std::optional<bool> canSendPhotos;
    std::optional<bool> canSendVideos;
    std::optional<bool> canSendVideoNotes;
    std::optional<bool> canSendVoiceNotes;
    std::optional<bool> canSendPolls;
    std::optional<bool> canSendOtherMessages;
    std::optional<bool> canAddWebPagePreviews;
    std::optional<bool> canChangeInfo;
    std::optional<bool> canInviteUsers;
    std::optional<bool> canPinMessages;
    std::optional<bool> canManageTopics;
};

/**
 * @brief Base class for reaction types
 */
class ReactionType {
   public:
    using Ptr = ReactionType*;
    virtual ~ReactionType() = default;
};

/**
 * @brief This object represents one result of an inline query.
 */
class InlineQueryResult {
   public:
    using Ptr = InlineQueryResult*;
    virtual ~InlineQueryResult() = default;

    std::string type;
    std::string id;
};

}  // namespace tgbot_api
