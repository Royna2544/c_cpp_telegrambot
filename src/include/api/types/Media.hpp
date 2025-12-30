#pragma once

#include "forwards.hpp"

namespace tgbot_api {

/**
 * @brief This object represents an animation file (GIF or H.264/MPEG-4 AVC
 * video without sound).
 */
class Animation {
   public:
    using Ptr = Animation*;

    std::string fileId;
    std::string fileUniqueId;
    std::int32_t width{};
    std::int32_t height{};
    std::int32_t duration{};
    std::optional<std::string> fileName;
    std::optional<std::string> mimeType;
    std::optional<std::int64_t> fileSize;
};

/**
 * @brief This object represents one size of a photo or a file/sticker
 * thumbnail.
 */
class PhotoSize {
   public:
    using Ptr = PhotoSize*;

    std::string fileId;
    std::string fileUniqueId;
    std::int32_t width{};
    std::int32_t height{};
    std::optional<std::int64_t> fileSize;
};

/**
 * @brief This object represents a sticker.
 */
class Sticker {
   public:
    using Ptr = Sticker*;

    enum class Type { Regular, Mask, CustomEmoji };

    std::string fileId;
    std::string fileUniqueId;
    Type type{Type::Regular};
    std::int32_t width{};
    std::int32_t height{};
    bool isAnimated{false};
    bool isVideo{false};
    std::optional<std::string> emoji;
    std::optional<std::string> setName;
    std::optional<std::int64_t> fileSize;
};

/**
 * @brief This object represents a video file.
 */
class Video {
   public:
    using Ptr = Video*;

    std::string fileId;
    std::string fileUniqueId;
    std::int32_t width{};
    std::int32_t height{};
    std::int32_t duration{};
    std::optional<std::string> fileName;
    std::optional<std::string> mimeType;
    std::optional<std::int64_t> fileSize;
};

/**
 * @brief This object represents a general file.
 */
class Document {
   public:
    using Ptr = Document*;

    std::string fileId;
    std::string fileUniqueId;
    std::optional<std::string> fileName;
    std::optional<std::string> mimeType;
    std::optional<std::int64_t> fileSize;
};

/**
 * @brief This object represents a file ready to be downloaded.
 */
class File {
   public:
    using Ptr = File*;

    std::string fileId;
    std::string fileUniqueId;
    std::optional<std::int64_t> fileSize;
    std::optional<std::string> filePath;
};

/**
 * @brief This object represents one special entity in a text message.
 */
class MessageEntity {
   public:
    using Ptr = MessageEntity*;

    struct Type {
        static constexpr const char* Mention = "mention";
        static constexpr const char* Hashtag = "hashtag";
        static constexpr const char* Cashtag = "cashtag";
        static constexpr const char* BotCommand = "bot_command";
        static constexpr const char* Url = "url";
        static constexpr const char* Email = "email";
        static constexpr const char* PhoneNumber = "phone_number";
        static constexpr const char* Bold = "bold";
        static constexpr const char* Italic = "italic";
        static constexpr const char* Underline = "underline";
        static constexpr const char* Strikethrough = "strikethrough";
        static constexpr const char* Spoiler = "spoiler";
        static constexpr const char* Code = "code";
        static constexpr const char* Pre = "pre";
        static constexpr const char* TextLink = "text_link";
        static constexpr const char* TextMention = "text_mention";
        static constexpr const char* CustomEmoji = "custom_emoji";
    };

    std::string type;
    std::int32_t offset{};
    std::int32_t length{};
    std::optional<std::string> url;
    std::optional<User::Ptr> user;
    std::optional<std::string> language;
    std::optional<std::string> customEmojiId;
};

}  // namespace tgbot_api
