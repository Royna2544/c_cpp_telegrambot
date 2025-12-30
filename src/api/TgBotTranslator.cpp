#include "api/TgBotTranslator.hpp"

#include <absl/log/log.h>

namespace TgBotTranslator {

// ============================================================================
// Forward conversions: TgBot -> tgbot_api
// ============================================================================

tgbot_api::User::Ptr fromTgBot(const TgBot::User::Ptr& tgUser) {
    if (!tgUser) {
        return nullptr;
    }

    auto apiUser = std::make_shared<tgbot_api::User>();
    apiUser->id = tgUser->id;
    apiUser->isBot = tgUser->isBot;
    apiUser->firstName = tgUser->firstName;
    apiUser->lastName = tgUser->lastName;
    apiUser->username = tgUser->username;
    apiUser->languageCode = tgUser->languageCode;
    apiUser->isPremium = tgUser->isPremium;
    apiUser->addedToAttachmentMenu = tgUser->addedToAttachmentMenu;
    apiUser->canJoinGroups = tgUser->canJoinGroups;
    apiUser->canReadAllGroupMessages = tgUser->canReadAllGroupMessages;
    apiUser->supportsInlineQueries = tgUser->supportsInlineQueries;
    apiUser->canConnectToBusiness = tgUser->canConnectToBusiness;

    return apiUser;
}

tgbot_api::Chat::Type fromTgBot(TgBot::Chat::Type tgType) {
    switch (tgType) {
        case TgBot::Chat::Type::Private:
            return tgbot_api::Chat::Type::Private;
        case TgBot::Chat::Type::Group:
            return tgbot_api::Chat::Type::Group;
        case TgBot::Chat::Type::Supergroup:
            return tgbot_api::Chat::Type::Supergroup;
        case TgBot::Chat::Type::Channel:
            return tgbot_api::Chat::Type::Channel;
    }
    return tgbot_api::Chat::Type::Private;
}

tgbot_api::Chat::Ptr fromTgBot(const TgBot::Chat::Ptr& tgChat) {
    if (!tgChat) {
        return nullptr;
    }

    auto apiChat = std::make_shared<tgbot_api::Chat>();
    apiChat->id = tgChat->id;
    apiChat->type = fromTgBot(tgChat->type);
    apiChat->title = tgChat->title;
    apiChat->username = tgChat->username;
    apiChat->firstName = tgChat->firstName;
    apiChat->lastName = tgChat->lastName;
    apiChat->isForum = tgChat->isForum;

    return apiChat;
}

tgbot_api::MessageEntity::Ptr fromTgBot(
    const TgBot::MessageEntity::Ptr& tgEntity) {
    if (!tgEntity) {
        return nullptr;
    }

    auto apiEntity = std::make_shared<tgbot_api::MessageEntity>();
    apiEntity->type = tgEntity->type;
    apiEntity->offset = tgEntity->offset;
    apiEntity->length = tgEntity->length;
    apiEntity->url = tgEntity->url;
    apiEntity->user = fromTgBot(tgEntity->user);
    apiEntity->language = tgEntity->language;
    apiEntity->customEmojiId = tgEntity->customEmojiId;

    return apiEntity;
}

tgbot_api::Animation::Ptr fromTgBot(const TgBot::Animation::Ptr& tgAnimation) {
    if (!tgAnimation) {
        return nullptr;
    }

    auto apiAnimation = std::make_shared<tgbot_api::Animation>();
    apiAnimation->fileId = tgAnimation->fileId;
    apiAnimation->fileUniqueId = tgAnimation->fileUniqueId;
    apiAnimation->width = tgAnimation->width;
    apiAnimation->height = tgAnimation->height;
    apiAnimation->duration = tgAnimation->duration;
    apiAnimation->fileName = tgAnimation->fileName;
    apiAnimation->mimeType = tgAnimation->mimeType;
    apiAnimation->fileSize = tgAnimation->fileSize;

    return apiAnimation;
}

tgbot_api::PhotoSize::Ptr fromTgBot(const TgBot::PhotoSize::Ptr& tgPhoto) {
    if (!tgPhoto) {
        return nullptr;
    }

    auto apiPhoto = std::make_shared<tgbot_api::PhotoSize>();
    apiPhoto->fileId = tgPhoto->fileId;
    apiPhoto->fileUniqueId = tgPhoto->fileUniqueId;
    apiPhoto->width = tgPhoto->width;
    apiPhoto->height = tgPhoto->height;
    apiPhoto->fileSize = tgPhoto->fileSize;

    return apiPhoto;
}

tgbot_api::Sticker::Type fromTgBot(TgBot::Sticker::Type tgType) {
    switch (tgType) {
        case TgBot::Sticker::Type::Regular:
            return tgbot_api::Sticker::Type::Regular;
        case TgBot::Sticker::Type::Mask:
            return tgbot_api::Sticker::Type::Mask;
        case TgBot::Sticker::Type::CustomEmoji:
            return tgbot_api::Sticker::Type::CustomEmoji;
    }
    return tgbot_api::Sticker::Type::Regular;
}

tgbot_api::Sticker::Ptr fromTgBot(const TgBot::Sticker::Ptr& tgSticker) {
    if (!tgSticker) {
        return nullptr;
    }

    auto apiSticker = std::make_shared<tgbot_api::Sticker>();
    apiSticker->fileId = tgSticker->fileId;
    apiSticker->fileUniqueId = tgSticker->fileUniqueId;
    apiSticker->type = fromTgBot(tgSticker->type);
    apiSticker->width = tgSticker->width;
    apiSticker->height = tgSticker->height;
    apiSticker->isAnimated = tgSticker->isAnimated;
    apiSticker->isVideo = tgSticker->isVideo;
    apiSticker->emoji = tgSticker->emoji;
    apiSticker->setName = tgSticker->setName;
    apiSticker->fileSize = tgSticker->fileSize;

    return apiSticker;
}

tgbot_api::Video::Ptr fromTgBot(const TgBot::Video::Ptr& tgVideo) {
    if (!tgVideo) {
        return nullptr;
    }

    auto apiVideo = std::make_shared<tgbot_api::Video>();
    apiVideo->fileId = tgVideo->fileId;
    apiVideo->fileUniqueId = tgVideo->fileUniqueId;
    apiVideo->width = tgVideo->width;
    apiVideo->height = tgVideo->height;
    apiVideo->duration = tgVideo->duration;
    apiVideo->fileName = tgVideo->fileName;
    apiVideo->mimeType = tgVideo->mimeType;
    apiVideo->fileSize = tgVideo->fileSize;

    return apiVideo;
}

tgbot_api::Document::Ptr fromTgBot(const TgBot::Document::Ptr& tgDocument) {
    if (!tgDocument) {
        return nullptr;
    }

    auto apiDocument = std::make_shared<tgbot_api::Document>();
    apiDocument->fileId = tgDocument->fileId;
    apiDocument->fileUniqueId = tgDocument->fileUniqueId;
    apiDocument->fileName = tgDocument->fileName;
    apiDocument->mimeType = tgDocument->mimeType;
    apiDocument->fileSize = tgDocument->fileSize;

    return apiDocument;
}

tgbot_api::File::Ptr fromTgBot(const TgBot::File::Ptr& tgFile) {
    if (!tgFile) {
        return nullptr;
    }

    auto apiFile = std::make_shared<tgbot_api::File>();
    apiFile->fileId = tgFile->fileId;
    apiFile->fileUniqueId = tgFile->fileUniqueId;
    apiFile->fileSize = tgFile->fileSize;
    apiFile->filePath = tgFile->filePath;

    return apiFile;
}

tgbot_api::Message::Ptr fromTgBot(const TgBot::Message::Ptr& tgMessage,
                                   tgbot_api::SplitMessageText how) {
    if (!tgMessage) {
        return nullptr;
    }

    auto apiMessage = std::make_shared<tgbot_api::Message>();

    // Basic fields
    apiMessage->messageId = tgMessage->messageId;
    apiMessage->messageThreadId = tgMessage->messageThreadId;
    apiMessage->from = fromTgBot(tgMessage->from);
    apiMessage->senderChat = fromTgBot(tgMessage->senderChat);
    apiMessage->date = tgMessage->date;
    apiMessage->chat = fromTgBot(tgMessage->chat);
    if (apiMessage->chat) {
        apiMessage->chat_id = apiMessage->chat->id;
    }
    apiMessage->text = tgMessage->text;
    apiMessage->isTopicMessage = tgMessage->isTopicMessage;

    // Convert entities
    for (const auto& entity : tgMessage->entities) {
        apiMessage->entities.push_back(fromTgBot(entity));
    }

    // Convert photos
    for (const auto& photo : tgMessage->photo) {
        apiMessage->photo.push_back(fromTgBot(photo));
    }

    // Media
    apiMessage->animation = fromTgBot(tgMessage->animation);
    apiMessage->document = fromTgBot(tgMessage->document);
    apiMessage->sticker = fromTgBot(tgMessage->sticker);
    apiMessage->video = fromTgBot(tgMessage->video);

    // Reply message
    apiMessage->replyToMessage = fromTgBot(tgMessage->replyToMessage, how);

    // Parse the message text
    apiMessage->parseText(how);

    return apiMessage;
}

tgbot_api::StickerSet::Ptr fromTgBot(const TgBot::StickerSet::Ptr& tgSet) {
    if (!tgSet) {
        return nullptr;
    }

    auto apiSet = std::make_shared<tgbot_api::StickerSet>();
    apiSet->name = tgSet->name;
    apiSet->title = tgSet->title;
    apiSet->stickerType = fromTgBot(tgSet->stickerType);

    for (const auto& sticker : tgSet->stickers) {
        apiSet->stickers.push_back(fromTgBot(sticker));
    }

    return apiSet;
}

// ============================================================================
// Reverse conversions: tgbot_api -> TgBot
// ============================================================================

TgBot::User::Ptr toTgBot(const tgbot_api::User::Ptr& apiUser) {
    if (!apiUser) {
        return nullptr;
    }

    auto tgUser = std::make_shared<TgBot::User>();
    tgUser->id = apiUser->id;
    tgUser->isBot = apiUser->isBot;
    tgUser->firstName = apiUser->firstName;
    tgUser->lastName = apiUser->lastName;
    tgUser->username = apiUser->username;
    tgUser->languageCode = apiUser->languageCode;
    tgUser->isPremium = apiUser->isPremium;
    tgUser->addedToAttachmentMenu = apiUser->addedToAttachmentMenu;
    tgUser->canJoinGroups = apiUser->canJoinGroups;
    tgUser->canReadAllGroupMessages = apiUser->canReadAllGroupMessages;
    tgUser->supportsInlineQueries = apiUser->supportsInlineQueries;
    tgUser->canConnectToBusiness = apiUser->canConnectToBusiness;

    return tgUser;
}

TgBot::Chat::Type toTgBot(tgbot_api::Chat::Type apiType) {
    switch (apiType) {
        case tgbot_api::Chat::Type::Private:
            return TgBot::Chat::Type::Private;
        case tgbot_api::Chat::Type::Group:
            return TgBot::Chat::Type::Group;
        case tgbot_api::Chat::Type::Supergroup:
            return TgBot::Chat::Type::Supergroup;
        case tgbot_api::Chat::Type::Channel:
            return TgBot::Chat::Type::Channel;
    }
    return TgBot::Chat::Type::Private;
}

TgBot::Chat::Ptr toTgBot(const tgbot_api::Chat::Ptr& apiChat) {
    if (!apiChat) {
        return nullptr;
    }

    auto tgChat = std::make_shared<TgBot::Chat>();
    tgChat->id = apiChat->id;
    tgChat->type = toTgBot(apiChat->type);
    tgChat->title = apiChat->title;
    tgChat->username = apiChat->username;
    tgChat->firstName = apiChat->firstName;
    tgChat->lastName = apiChat->lastName;
    tgChat->isForum = apiChat->isForum;

    return tgChat;
}

TgBot::Message::Ptr toTgBot(const tgbot_api::Message::Ptr& apiMessage) {
    if (!apiMessage) {
        return nullptr;
    }

    auto tgMessage = std::make_shared<TgBot::Message>();
    tgMessage->messageId = apiMessage->messageId;
    tgMessage->messageThreadId = apiMessage->messageThreadId;
    tgMessage->from = toTgBot(apiMessage->from);
    tgMessage->senderChat = toTgBot(apiMessage->senderChat);
    tgMessage->date = apiMessage->date;
    tgMessage->chat = toTgBot(apiMessage->chat);
    tgMessage->text = apiMessage->text;
    tgMessage->isTopicMessage = apiMessage->isTopicMessage;

    return tgMessage;
}

TgBot::InputFile::Ptr toTgBot(const tgbot_api::InputFile::Ptr& apiInputFile) {
    if (!apiInputFile) {
        return nullptr;
    }

    auto tgInputFile = std::make_shared<TgBot::InputFile>();
    tgInputFile->data = apiInputFile->data;
    tgInputFile->mimeType = apiInputFile->mimeType;
    tgInputFile->fileName = apiInputFile->fileName;

    return tgInputFile;
}

TgBot::InputSticker::Ptr toTgBot(
    const tgbot_api::InputSticker::Ptr& apiInputSticker) {
    if (!apiInputSticker) {
        return nullptr;
    }

    auto tgInputSticker = std::make_shared<TgBot::InputSticker>();
    tgInputSticker->sticker = toTgBot(apiInputSticker->sticker);
    tgInputSticker->format = apiInputSticker->format;
    tgInputSticker->emojiList = apiInputSticker->emojiList;

    return tgInputSticker;
}

TgBot::ReplyParameters::Ptr toTgBot(
    const tgbot_api::ReplyParameters::Ptr& apiReplyParams) {
    if (!apiReplyParams) {
        return nullptr;
    }

    auto tgReplyParams = std::make_shared<TgBot::ReplyParameters>();
    tgReplyParams->messageId = apiReplyParams->messageId;
    tgReplyParams->allowSendingWithoutReply =
        apiReplyParams->allowSendingWithoutReply;

    return tgReplyParams;
}

TgBot::GenericReply::Ptr toTgBot(
    const tgbot_api::GenericReply::Ptr& apiReply) {
    if (!apiReply) {
        return nullptr;
    }

    // Try to downcast to specific types
    if (auto inlineKb =
            std::dynamic_pointer_cast<tgbot_api::InlineKeyboardMarkup>(
                apiReply)) {
        return toTgBot(inlineKb);
    }

    if (auto replyKb =
            std::dynamic_pointer_cast<tgbot_api::ReplyKeyboardMarkup>(
                apiReply)) {
        auto tgReplyKb = std::make_shared<TgBot::ReplyKeyboardMarkup>();
        // Convert keyboard buttons (simplified)
        return tgReplyKb;
    }

    if (auto removeKb =
            std::dynamic_pointer_cast<tgbot_api::ReplyKeyboardRemove>(
                apiReply)) {
        auto tgRemoveKb = std::make_shared<TgBot::ReplyKeyboardRemove>();
        tgRemoveKb->removeKeyboard = removeKb->removeKeyboard;
        tgRemoveKb->selective = removeKb->selective;
        return tgRemoveKb;
    }

    if (auto forceReply =
            std::dynamic_pointer_cast<tgbot_api::ForceReply>(apiReply)) {
        auto tgForceReply = std::make_shared<TgBot::ForceReply>();
        tgForceReply->forceReply = forceReply->forceReply;
        tgForceReply->inputFieldPlaceholder =
            forceReply->inputFieldPlaceholder;
        tgForceReply->selective = forceReply->selective;
        return tgForceReply;
    }

    return nullptr;
}

TgBot::ChatPermissions::Ptr toTgBot(
    const tgbot_api::ChatPermissions::Ptr& apiPermissions) {
    if (!apiPermissions) {
        return nullptr;
    }

    auto tgPermissions = std::make_shared<TgBot::ChatPermissions>();
    tgPermissions->canSendMessages = apiPermissions->canSendMessages;
    tgPermissions->canSendAudios = apiPermissions->canSendAudios;
    tgPermissions->canSendDocuments = apiPermissions->canSendDocuments;
    tgPermissions->canSendPhotos = apiPermissions->canSendPhotos;
    tgPermissions->canSendVideos = apiPermissions->canSendVideos;
    tgPermissions->canSendVideoNotes = apiPermissions->canSendVideoNotes;
    tgPermissions->canSendVoiceNotes = apiPermissions->canSendVoiceNotes;
    tgPermissions->canSendPolls = apiPermissions->canSendPolls;
    tgPermissions->canSendOtherMessages = apiPermissions->canSendOtherMessages;
    tgPermissions->canAddWebPagePreviews =
        apiPermissions->canAddWebPagePreviews;
    tgPermissions->canChangeInfo = apiPermissions->canChangeInfo;
    tgPermissions->canInviteUsers = apiPermissions->canInviteUsers;
    tgPermissions->canPinMessages = apiPermissions->canPinMessages;
    tgPermissions->canManageTopics = apiPermissions->canManageTopics;

    return tgPermissions;
}

TgBot::InlineKeyboardMarkup::Ptr toTgBot(
    const tgbot_api::InlineKeyboardMarkup::Ptr& apiMarkup) {
    if (!apiMarkup) {
        return nullptr;
    }

    auto tgMarkup = std::make_shared<TgBot::InlineKeyboardMarkup>();

    for (const auto& row : apiMarkup->inlineKeyboard) {
        std::vector<TgBot::InlineKeyboardButton::Ptr> tgRow;
        for (const auto& button : row) {
            auto tgButton = std::make_shared<TgBot::InlineKeyboardButton>();
            tgButton->text = button->text;
            tgButton->url = button->url;
            tgButton->callbackData = button->callbackData;
            tgRow.push_back(tgButton);
        }
        tgMarkup->inlineKeyboard.push_back(tgRow);
    }

    return tgMarkup;
}

TgBot::ReactionType::Ptr toTgBot(
    const tgbot_api::ReactionType::Ptr& apiReaction) {
    // Simplified: Just return a base pointer
    // In reality, you'd need to handle specific reaction types
    if (!apiReaction) {
        return nullptr;
    }
    return std::make_shared<TgBot::ReactionType>();
}

std::vector<TgBot::ReactionType::Ptr> toTgBot(
    const std::vector<tgbot_api::ReactionType::Ptr>& apiReactions) {
    std::vector<TgBot::ReactionType::Ptr> tgReactions;
    for (const auto& reaction : apiReactions) {
        tgReactions.push_back(toTgBot(reaction));
    }
    return tgReactions;
}

TgBot::Sticker::Type toTgBot(tgbot_api::Sticker::Type apiType) {
    switch (apiType) {
        case tgbot_api::Sticker::Type::Regular:
            return TgBot::Sticker::Type::Regular;
        case tgbot_api::Sticker::Type::Mask:
            return TgBot::Sticker::Type::Mask;
        case tgbot_api::Sticker::Type::CustomEmoji:
            return TgBot::Sticker::Type::CustomEmoji;
    }
    return TgBot::Sticker::Type::Regular;
}

std::vector<TgBot::InputSticker::Ptr> toTgBot(
    const std::vector<tgbot_api::InputSticker::Ptr>& apiStickers) {
    std::vector<TgBot::InputSticker::Ptr> tgStickers;
    for (const auto& sticker : apiStickers) {
        tgStickers.push_back(toTgBot(sticker));
    }
    return tgStickers;
}

}  // namespace TgBotTranslator
