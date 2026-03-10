#include <absl/log/log.h>
#include <tgbot/types/MessageOriginChannel.h>
#include <tgbot/types/MessageOriginChat.h>
#include <tgbot/types/MessageOriginHiddenUser.h>
#include <tgbot/types/MessageOriginUser.h>

#include <LogSinks.hpp>
#include <api/CommandModule.hpp>
#include <api/TgBotApi.hpp>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

#include "api/MessageExt.hpp"
#include "api/builtin_modules/builder/CurlUtils.hpp"
#include "tgbot/types/MessageOrigin.h"

constexpr std::string_view kRemoteApi =
    "https://bot.lyo.su/quote/generate.webp";

struct GenerateRequest {
    std::string type = "quote";
    std::string format = "webp";
    std::string backgroundColor;
    struct Message {
        struct User {
            UserId id;
            std::string name;
            std::string username;
            struct Photo {
                std::string big_file_id;
            } photo;
        } from;
        std::string text;
        bool avatar = true;
        struct ReplyTo {
            std::string name;
            std::string text;
            ChatId chat_id;
            User from;
        } replyMessage;
        struct Media {
            std::string file_id;
            int width, height;
            bool is_animated;
        };
        std::vector<Media> media;
    };
    std::vector<Message> messages;
};

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GenerateRequest, type, format,
                                   backgroundColor, messages)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GenerateRequest::Message::User::Photo,
                                   big_file_id)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GenerateRequest::Message::User, id, name,
                                   username, photo)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GenerateRequest::Message::ReplyTo, name,
                                   text, chat_id, from)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GenerateRequest::Message, from, text, avatar,
                                   replyMessage, media)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(GenerateRequest::Message::Media, file_id,
                                   width, height, is_animated)

DECLARE_COMMAND_HANDLER(q) {
    if (!message->reply()->exists()) {
        api->sendReplyMessage(message->message(),
                              "Please reply to a message to quote it!");
        return;
    }
    auto origin = message->reply()->message()->forwardOrigin;
    auto user = message->reply()->get<MessageAttrs::User>();
    if (!origin) {
        // Empty body...
    } else if (origin->type == TgBot::MessageOriginUser::TYPE) {
        auto ptr = std::static_pointer_cast<TgBot::MessageOriginUser>(origin);
        user = ptr->senderUser;
    } else if (origin->type == TgBot::MessageOriginHiddenUser::TYPE) {
        auto ptr =
            std::static_pointer_cast<TgBot::MessageOriginHiddenUser>(origin);
        // We can't obtain the user, but we can at least get the name.
        user = std::make_shared<TgBot::User>();
        user->id = 0;
        user->firstName = ptr->senderUserName;
    } else if (origin->type == TgBot::MessageOriginChat::TYPE) {
        auto ptr = std::static_pointer_cast<TgBot::MessageOriginChat>(origin);
        user = std::make_shared<TgBot::User>();
        user->id = ptr->senderChat->id;
        user->firstName = ptr->senderChat->title.value_or("");
    } else if (origin->type == TgBot::MessageOriginChannel::TYPE) {
        auto ptr =
            std::static_pointer_cast<TgBot::MessageOriginChannel>(origin);
        user = std::make_shared<TgBot::User>();
        user->id = ptr->chat->id;
        user->firstName = ptr->chat->title.value_or("");
    } else {
        // Give up.
        api->sendMessage(message->get<MessageAttrs::Chat>(),
                         "Unsupported message origin type: " + origin->type);
        return;
    }

    enum class MessageType : uint8_t {
        Text,
        Sticker,
        Photo,
        Video,
        Animation,
    } type = MessageType::Text;

    auto text = message->reply()->message()->text.value_or("");
    if (message->message()->quote) {
        text = message->message()->quote->text;
    }
    std::string media;
    if (message->reply()->has<MessageAttrs::Sticker>()) {
        auto _sticker = message->reply()->get<MessageAttrs::Sticker>();
        if (!_sticker->thumbnail) {
            media = _sticker->fileId;
        } else {
            // Grab thumbnail if the sticker is animated, since we don't support
            // animated stickers for now.
            media = _sticker->thumbnail->fileId;
        }
        type = MessageType::Sticker;
    } else if (message->reply()->has<MessageAttrs::Photo>()) {
        auto photo = message->reply()->get<MessageAttrs::Photo>();
        media = photo->fileId;
        type = MessageType::Photo;
    } else if (message->reply()->has<MessageAttrs::Video>()) {
        auto video = message->reply()->get<MessageAttrs::Video>();
        media = video->thumbnail->fileId;
        type = MessageType::Video;
    } else if (message->reply()->has<MessageAttrs::Animation>()) {
        auto animation = message->reply()->get<MessageAttrs::Animation>();
        if (!animation->thumbnail) {
            api->sendMessage(
                message->get<MessageAttrs::Chat>(),
                "Unsupported media type: animation without thumbnail");
            return;
        }
        media = animation->thumbnail->fileId;
        type = MessageType::Animation;
    }

    // Now, let us be clear.
    // If the user /q a message without any words, we are talking about the
    // replied-to-message
    std::string args = message->get<MessageAttrs::ExtraText>();
    if (!args.empty()) {
        // Support text=<arg> to override the text.
        if (args.starts_with("text=")) {
            text = args.substr(5);
            media.clear();
        }
    }

    std::string username;
    if (user->lastName) {
        username = fmt::format("{} {}", user->firstName, *user->lastName);
    } else {
        username = user->firstName;
    }
    GenerateRequest req;
    req.messages.push_back({
        .from =
            {
                .id = user->id,
                .name = username,
                .username = user->username.value_or(""),
            },
        .text = text,
        .avatar = true,
        .media = std::vector<GenerateRequest::Message::Media>{{
            .file_id = media,
            .width = 30,
            .height = 30,
            .is_animated = false,
        }},
    });
    if (user->id > 0) {
        auto photos = api->getUserProfilePhotos(user->id);
        if (photos->totalCount != 0) {
            const auto& photoSizes = photos->photos.front();
            const auto& largestPhoto = photoSizes.back();

            req.messages[0].from.photo.big_file_id = largestPhoto->fileId;
        }
    } else if (user->id < 0) {
        auto chat = api->getChat(user->id);
        if (chat->photo) {
            req.messages[0].from.photo.big_file_id = chat->photo->bigFileId;
        }
    }
    if (type == MessageType::Sticker) {
        req.backgroundColor = "transparent";
    }

    DLOG(INFO) << "Generated request JSON: " << nlohmann::json(req).dump();
    nlohmann::json j = req;
    auto ret = CurlUtils::send_json_get_reply(kRemoteApi, j.dump());
    if (!ret) {
        api->sendMessage(message->get<MessageAttrs::Chat>(),
                         "Failed to generate quote!");
        return;
    }
    auto file = std::make_shared<InputFile>();
    file->fileName = "sticker.wedp";
    file->data = std::move(*ret);
    api->sendSticker(message->get<MessageAttrs::Chat>(), std::move(file));
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::HideDescription | DynModule::Flags::Enforced,
    .name = "q",
    .description = "Quote a message",
    .function = COMMAND_HANDLER_NAME(q),
};
