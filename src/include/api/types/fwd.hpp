#pragma once

#include <api/types/GenericReply.hpp>
#include <api/types/InlineQueryResults.hpp>

namespace api::types {

struct Animation;
struct BotCommand;
struct CallbackQuery;
struct ChatBoostAdded;
struct ChatPhoto;
struct ChatPermissions;
struct BirthDate;
struct Chat;
struct Dice;
struct Document;
struct ExternalReplyInfo;
struct File;
struct ForceReply;
struct ForumTopicReopened;
struct ForumTopicEdited;
struct ForumTopicCreated;
struct ForumTopicClosed;
struct GeneralForumTopicHidden;
struct GeneralForumTopicUnhidden;
struct InlineKeyboardButton;
struct InlineKeyboardMarkup;
struct InlineQuery;
struct InputFile;
struct InputSticker;
struct KeyboardButton;
struct KeyboardButtonPollType;
struct LinkPreviewOptions;
struct MaskPosition;
struct Message;
struct MessageAutoDeleteTimerChanged;
struct MessageEntity;
struct ParsedMessage;  // API_ADDED: Extension for Message parsing
struct PhotoSize;
struct Poll;
struct PollOption;
struct ReactionTypeEmoji;
struct ReactionTypeCustomEmoji;
struct ReplyKeyboardMarkup;
struct ReplyKeyboardRemove;
struct Sticker;
struct StickerSet;
struct SwitchInlineQueryChosenChat;
struct TextQuote;
struct User;
struct Video;
struct VideoChatStarted;
struct VideoChatEnded;
struct VideoChatParticipantsInvited;
struct VideoChatScheduled;

}  // namespace api::types