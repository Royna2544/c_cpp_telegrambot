#include <fmt/core.h>

#include <api/CommandModule.hpp>
#include <api/StringResLoader.hpp>
#include <api/TgBotApi.hpp>

#include "api/MessageExt.hpp"

DECLARE_COMMAND_HANDLER(fileid) {
    std::string file;
    std::string unifile;
    std::optional<std::string> thumbnail;

    if (message->reply()->exists()) {
        const auto reply = message->reply();
        if (reply->has<MessageAttrs::Sticker>()) {
            file = reply->get<MessageAttrs::Sticker>()->fileId;
            unifile = reply->get<MessageAttrs::Sticker>()->fileUniqueId;
            thumbnail = reply->get<MessageAttrs::Sticker>()->thumbnail ? std::make_optional(
                            reply->get<MessageAttrs::Sticker>()->thumbnail->fileId)
                    : std::nullopt;
        } else if (reply->has<MessageAttrs::Animation>()) {
            file = reply->get<MessageAttrs::Animation>()->fileId;
            unifile = reply->get<MessageAttrs::Animation>()->fileUniqueId;
            thumbnail = reply->get<MessageAttrs::Animation>()->thumbnail
                            ? std::make_optional(
                                  reply->get<MessageAttrs::Animation>()
                                      ->thumbnail->fileId)
                    : std::nullopt;
        } else if (reply->has<MessageAttrs::Photo>()) {
            file = reply->get<MessageAttrs::Photo>()->fileId;
            unifile = reply->get<MessageAttrs::Photo>()->fileUniqueId;
        } else if (reply->has<MessageAttrs::Video>()) {
            file = reply->get<MessageAttrs::Video>()->fileId;
            unifile = reply->get<MessageAttrs::Video>()->fileUniqueId;
            thumbnail =
                reply->get<MessageAttrs::Video>()->thumbnail
                    ? std::make_optional(
                          reply->get<MessageAttrs::Video>()->thumbnail->fileId)
                    : std::nullopt;
        } else if (reply->has<MessageAttrs::Document>()) {
            file = reply->get<MessageAttrs::Document>()->fileId;
            unifile = reply->get<MessageAttrs::Document>()->fileUniqueId;
        } else {
            file = unifile = res->get(Strings::UNKNOWN);
        }
        if (thumbnail) {
            api->sendReplyMessage<TgBotApi::ParseMode::Markdown>(
                message->message(),
                fmt::format(
                    "FileId: `{}`\nFileUniqueId: `{}`\nThumbnail FileId: `{}`",
                    file, unifile, *thumbnail));
        } else {
            api->sendReplyMessage<TgBotApi::ParseMode::Markdown>(
                message->message(),
                fmt::format("FileId: `{}`\nFileUniqueId: `{}`", file, unifile));
        }
    } else {
        api->sendReplyMessage(message->message(),
                              res->get(Strings::REPLY_TO_A_MEDIA));
    }
}

extern "C" DYN_COMMAND_EXPORT const struct DynModule DYN_COMMAND_SYM = {
    .flags = DynModule::Flags::None,
    .name = "fileid",
    .description = "Get fileId of a media",
    .function = COMMAND_HANDLER_NAME(fileid),
    .valid_args = {
        .enabled = true,
        .counts = DynModule::craftArgCountMask<0>(),
        .split_type = DynModule::ValidArgs::Split::None,
        .usage = "<reply-to-a-media>",
    }};
