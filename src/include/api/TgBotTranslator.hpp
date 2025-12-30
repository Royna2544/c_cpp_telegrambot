#pragma once

#include <tgbot/types/Animation.h>
#include <tgbot/types/Chat.h>
#include <tgbot/types/ChatPermissions.h>
#include <tgbot/types/Document.h>
#include <tgbot/types/File.h>
#include <tgbot/types/ForceReply.h>
#include <tgbot/types/GenericReply.h>
#include <tgbot/types/InlineKeyboardMarkup.h>
#include <tgbot/types/InlineQueryResult.h>
#include <tgbot/types/InputFile.h>
#include <tgbot/types/InputSticker.h>
#include <tgbot/types/Message.h>
#include <tgbot/types/MessageEntity.h>
#include <tgbot/types/PhotoSize.h>
#include <tgbot/types/ReactionType.h>
#include <tgbot/types/ReplyKeyboardMarkup.h>
#include <tgbot/types/ReplyKeyboardRemove.h>
#include <tgbot/types/ReplyParameters.h>
#include <tgbot/types/Sticker.h>
#include <tgbot/types/StickerSet.h>
#include <tgbot/types/User.h>
#include <tgbot/types/Video.h>

#include "types/Chat.hpp"
#include "types/Media.hpp"
#include "types/Message.hpp"
#include "types/TelegramTypes.hpp"
#include "types/User.hpp"

/**
 * @file TgBotTranslator.hpp
 * @brief Translation layer between project types and tgbot-cpp types
 *
 * This file provides conversion functions to translate between the project's
 * internal Telegram types (in tgbot_api namespace) and the external tgbot-cpp
 * library types (in TgBot namespace). This allows the project to decouple from
 * the tgbot-cpp library's type system.
 */

namespace TgBotTranslator {

// ============================================================================
// Forward conversions: TgBot -> tgbot_api
// ============================================================================

/**
 * @brief Converts TgBot::User to tgbot_api::User
 */
tgbot_api::User::Ptr fromTgBot(const TgBot::User::Ptr& tgUser);

/**
 * @brief Converts TgBot::Chat to tgbot_api::Chat
 */
tgbot_api::Chat::Ptr fromTgBot(const TgBot::Chat::Ptr& tgChat);

/**
 * @brief Converts TgBot::MessageEntity to tgbot_api::MessageEntity
 */
tgbot_api::MessageEntity::Ptr fromTgBot(
    const TgBot::MessageEntity::Ptr& tgEntity);

/**
 * @brief Converts TgBot::Animation to tgbot_api::Animation
 */
tgbot_api::Animation::Ptr fromTgBot(const TgBot::Animation::Ptr& tgAnimation);

/**
 * @brief Converts TgBot::PhotoSize to tgbot_api::PhotoSize
 */
tgbot_api::PhotoSize::Ptr fromTgBot(const TgBot::PhotoSize::Ptr& tgPhoto);

/**
 * @brief Converts TgBot::Sticker to tgbot_api::Sticker
 */
tgbot_api::Sticker::Ptr fromTgBot(const TgBot::Sticker::Ptr& tgSticker);

/**
 * @brief Converts TgBot::Video to tgbot_api::Video
 */
tgbot_api::Video::Ptr fromTgBot(const TgBot::Video::Ptr& tgVideo);

/**
 * @brief Converts TgBot::Document to tgbot_api::Document
 */
tgbot_api::Document::Ptr fromTgBot(const TgBot::Document::Ptr& tgDocument);

/**
 * @brief Converts TgBot::File to tgbot_api::File
 */
tgbot_api::File::Ptr fromTgBot(const TgBot::File::Ptr& tgFile);

/**
 * @brief Converts TgBot::Message to tgbot_api::Message
 */
tgbot_api::Message::Ptr fromTgBot(
    const TgBot::Message::Ptr& tgMessage,
    tgbot_api::SplitMessageText how = tgbot_api::SplitMessageText::None);

/**
 * @brief Converts TgBot::StickerSet to tgbot_api::StickerSet
 */
tgbot_api::StickerSet::Ptr fromTgBot(const TgBot::StickerSet::Ptr& tgSet);

// ============================================================================
// Reverse conversions: tgbot_api -> TgBot
// ============================================================================

/**
 * @brief Converts tgbot_api::User to TgBot::User
 */
TgBot::User::Ptr toTgBot(const tgbot_api::User::Ptr& apiUser);

/**
 * @brief Converts tgbot_api::Chat to TgBot::Chat
 */
TgBot::Chat::Ptr toTgBot(const tgbot_api::Chat::Ptr& apiChat);

/**
 * @brief Converts tgbot_api::Message to TgBot::Message
 */
TgBot::Message::Ptr toTgBot(const tgbot_api::Message::Ptr& apiMessage);

/**
 * @brief Converts tgbot_api::InputFile to TgBot::InputFile
 */
TgBot::InputFile::Ptr toTgBot(const tgbot_api::InputFile::Ptr& apiInputFile);

/**
 * @brief Converts tgbot_api::InputSticker to TgBot::InputSticker
 */
TgBot::InputSticker::Ptr toTgBot(
    const tgbot_api::InputSticker::Ptr& apiInputSticker);

/**
 * @brief Converts tgbot_api::ReplyParameters to TgBot::ReplyParameters
 */
TgBot::ReplyParameters::Ptr toTgBot(
    const tgbot_api::ReplyParameters::Ptr& apiReplyParams);

/**
 * @brief Converts tgbot_api::GenericReply to TgBot::GenericReply
 */
TgBot::GenericReply::Ptr toTgBot(
    const tgbot_api::GenericReply::Ptr& apiReply);

/**
 * @brief Converts tgbot_api::ChatPermissions to TgBot::ChatPermissions
 */
TgBot::ChatPermissions::Ptr toTgBot(
    const tgbot_api::ChatPermissions::Ptr& apiPermissions);

/**
 * @brief Converts tgbot_api::InlineKeyboardMarkup to
 * TgBot::InlineKeyboardMarkup
 */
TgBot::InlineKeyboardMarkup::Ptr toTgBot(
    const tgbot_api::InlineKeyboardMarkup::Ptr& apiMarkup);

/**
 * @brief Converts tgbot_api::ReactionType to TgBot::ReactionType
 */
TgBot::ReactionType::Ptr toTgBot(
    const tgbot_api::ReactionType::Ptr& apiReaction);

/**
 * @brief Converts vector of tgbot_api::ReactionType to vector of
 * TgBot::ReactionType
 */
std::vector<TgBot::ReactionType::Ptr> toTgBot(
    const std::vector<tgbot_api::ReactionType::Ptr>& apiReactions);

/**
 * @brief Converts vector of tgbot_api::InputSticker to vector of
 * TgBot::InputSticker
 */
std::vector<TgBot::InputSticker::Ptr> toTgBot(
    const std::vector<tgbot_api::InputSticker::Ptr>& apiStickers);

// ============================================================================
// Sticker type conversions
// ============================================================================

/**
 * @brief Converts TgBot::Sticker::Type to tgbot_api::Sticker::Type
 */
tgbot_api::Sticker::Type fromTgBot(TgBot::Sticker::Type tgType);

/**
 * @brief Converts tgbot_api::Sticker::Type to TgBot::Sticker::Type
 */
TgBot::Sticker::Type toTgBot(tgbot_api::Sticker::Type apiType);

/**
 * @brief Converts TgBot::Chat::Type to tgbot_api::Chat::Type
 */
tgbot_api::Chat::Type fromTgBot(TgBot::Chat::Type tgType);

/**
 * @brief Converts tgbot_api::Chat::Type to TgBot::Chat::Type
 */
TgBot::Chat::Type toTgBot(tgbot_api::Chat::Type apiType);

}  // namespace TgBotTranslator
