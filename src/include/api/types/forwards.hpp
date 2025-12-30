#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/**
 * @file forwards.hpp
 * @brief Forward declarations for all project-specific Telegram types
 *
 * This file contains forward declarations to break circular dependencies
 * and reduce compilation times.
 */

namespace tgbot_api {

// Basic types
using UserId = std::int64_t;
using ChatId = std::int64_t;
using MessageId = std::int32_t;
using MessageThreadId = std::int32_t;

// Forward declarations of main types
class User;
class Chat;
class Message;
class Animation;
class Audio;
class Document;
class PhotoSize;
class Sticker;
class Video;
class VideoNote;
class Voice;
class Contact;
class Dice;
class File;
class MessageEntity;
class ReplyParameters;
class InputFile;
class InputSticker;
class StickerSet;
class GenericReply;
class InlineKeyboardMarkup;
class ReplyKeyboardMarkup;
class ReplyKeyboardRemove;
class ForceReply;
class InlineQueryResult;
class ChatPermissions;
class ReactionType;

// Pointer typedefs for convenience
template <typename T>
using Ptr = T*;

}  // namespace tgbot_api
