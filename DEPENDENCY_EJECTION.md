# Dependency Ejection from tgbot-cpp - Implementation Summary

## Overview

This document describes the dependency ejection architecture implemented to decouple the TgBot++ project from the tgbot-cpp library's type system. The goal is to create project-owned types that provide a stable API while hiding the tgbot-cpp implementation details behind a translation layer.

## Architecture

### 1. Project Type System (`tgbot_api` namespace)

All project-specific types are defined in the `tgbot_api` namespace located in `src/include/api/types/` and `src/api/types/`.

#### Core Types

**forwards.hpp**
- Forward declarations for all project types
- Basic type aliases (UserId, ChatId, MessageId, MessageThreadId)
- Shared pointer typedef template

**User.hpp**
- Represents a Telegram user or bot
- Fields: id, isBot, firstName, lastName, username, languageCode, isPremium, etc.
- Compatible with TgBot::User structure

**Chat.hpp**
- Represents a Telegram chat
- Includes Chat::Type enum (Private, Group, Supergroup, Channel)
- Fields: id, type, title, username, firstName, lastName, isForum
- Compatible with TgBot::Chat structure

**Message.hpp**
- **Combines standard Message fields with MessageExt parsing capabilities**
- Standard fields: messageId, from, chat, text, date, entities, photo, animation, sticker, video, document, etc.
- Extended fields: command, extraArgs, arguments, parsedReplyMessage
- Methods: parseText(), get<attr>(), has<attr>(), any(), is<attr>()
- Supports MessageAttrs enum for attribute access
- Includes internal::message::BotCommand struct
- **This replaces the old MessageExt class**

**Media.hpp**
- Animation: GIF or H.264/MPEG-4 AVC video
- PhotoSize: Photo or thumbnail size
- Sticker: Sticker with type enum (Regular, Mask, CustomEmoji)
- Video: Video file
- Document: General file
- File: Downloadable file reference
- MessageEntity: Text entity (mentions, hashtags, bot_command, etc.)

**TelegramTypes.hpp**
- ParseMode enum: None, Markdown, MarkdownV2, HTML
- StickerFormat enum: Static, Animated, Video
- InputFile: File upload representation
- InputSticker: Sticker to be added to set
- StickerSet: Sticker set with stickers
- GenericReply: Base class for reply markups
- InlineKeyboardMarkup: Inline keyboard
- ReplyKeyboardMarkup: Custom keyboard
- ReplyKeyboardRemove: Remove custom keyboard
- ForceReply: Force reply interface
- ReplyParameters: Reply message parameters
- ChatPermissions: Chat member permissions
- ReactionType: Base class for reactions
- InlineQueryResult: Base class for inline query results

### 2. Translation Layer

**TgBotTranslator.hpp / TgBotTranslator.cpp**

Provides bidirectional conversions between `TgBot::` types and `tgbot_api::` types.

#### Forward Conversions (TgBot → tgbot_api)

```cpp
tgbot_api::User::Ptr fromTgBot(const TgBot::User::Ptr&);
tgbot_api::Chat::Ptr fromTgBot(const TgBot::Chat::Ptr&);
tgbot_api::Message::Ptr fromTgBot(const TgBot::Message::Ptr&, SplitMessageText = None);
tgbot_api::Animation::Ptr fromTgBot(const TgBot::Animation::Ptr&);
tgbot_api::PhotoSize::Ptr fromTgBot(const TgBot::PhotoSize::Ptr&);
tgbot_api::Sticker::Ptr fromTgBot(const TgBot::Sticker::Ptr&);
tgbot_api::Video::Ptr fromTgBot(const TgBot::Video::Ptr&);
tgbot_api::Document::Ptr fromTgBot(const TgBot::Document::Ptr&);
tgbot_api::File::Ptr fromTgBot(const TgBot::File::Ptr&);
tgbot_api::StickerSet::Ptr fromTgBot(const TgBot::StickerSet::Ptr&);
tgbot_api::MessageEntity::Ptr fromTgBot(const TgBot::MessageEntity::Ptr&);
```

#### Reverse Conversions (tgbot_api → TgBot)

```cpp
TgBot::User::Ptr toTgBot(const tgbot_api::User::Ptr&);
TgBot::Chat::Ptr toTgBot(const tgbot_api::Chat::Ptr&);
TgBot::Message::Ptr toTgBot(const tgbot_api::Message::Ptr&);
TgBot::InputFile::Ptr toTgBot(const tgbot_api::InputFile::Ptr&);
TgBot::InputSticker::Ptr toTgBot(const tgbot_api::InputSticker::Ptr&);
TgBot::ReplyParameters::Ptr toTgBot(const tgbot_api::ReplyParameters::Ptr&);
TgBot::GenericReply::Ptr toTgBot(const tgbot_api::GenericReply::Ptr&);
TgBot::ChatPermissions::Ptr toTgBot(const tgbot_api::ChatPermissions::Ptr&);
TgBot::InlineKeyboardMarkup::Ptr toTgBot(const tgbot_api::InlineKeyboardMarkup::Ptr&);
```

#### Enum Conversions

- `fromTgBot(TgBot::Sticker::Type)` → `tgbot_api::Sticker::Type`
- `toTgBot(tgbot_api::Sticker::Type)` → `TgBot::Sticker::Type`
- `fromTgBot(TgBot::Chat::Type)` → `tgbot_api::Chat::Type`
- `toTgBot(tgbot_api::Chat::Type)` → `TgBot::Chat::Type`

### 3. Updated API Interfaces

**TgBotApi.hpp**
- Changed all type references from `TgBot::` to `tgbot_api::`
- Uses project types: User, Chat, Message, File, InputFile, InputSticker, Sticker, StickerSet, GenericReply, ReactionType, ChatPermissions
- ParseMode typedef now references `tgbot_api::ParseMode`
- StickerFormat references `tgbot_api::StickerFormat`
- InlineKeyboardMarkup references `tgbot_api::InlineKeyboardMarkup`
- **Note**: TgBot::InlineQueryResult and TgBot::EventBroadcaster types remain for internal use

**ReplyParametersExt.hpp**
- Now extends `tgbot_api::ReplyParameters` instead of `TgBot::ReplyParameters`
- Maintains messageThreadId field and hasThreadId() method

**Utils.hpp**
- MediaIds struct constructors now accept `tgbot_api::` media types
- ChatIds constructor accepts `tgbot_api::Chat::Ptr`
- Removed all TgBot includes

**AuthContext.hpp**
- Uses `tgbot_api::Message` and `tgbot_api::User`
- isAuthorized() methods accept project types
- isUnderTimeLimit() methods accept project Message type

**CommandModule.hpp**
- Command callback signature: `void (*)(TgBotApi::Ptr, tgbot_api::Message*, ...)`
- DECLARE_COMMAND_HANDLER macro updated to use `tgbot_api::Message*`
- ValidArgs::Split type is now `tgbot_api::SplitMessageText`
- All references to MessageExt replaced with tgbot_api::Message

### 4. Implementation Strategy (TgBotApiImpl)

**Next Steps for TgBotApiImpl.cpp:**

1. Include TgBotTranslator.hpp
2. In each `_impl` method:
   - Accept project types as parameters
   - Convert project types to TgBot types using `toTgBot()`
   - Call underlying TgBot::Api methods
   - Convert TgBot return types to project types using `fromTgBot()`
   - Return project types

Example pattern:
```cpp
Message::Ptr TgBotApiImpl::sendMessage_impl(
    ChatId chatId, const std::string_view text,
    ReplyParametersExt::Ptr replyParameters,
    GenericReply::Ptr replyMarkup,
    tgbot_api::ParseMode parseMode) const override {
    
    // Convert project types to TgBot types
    TgBot::ReplyParameters::Ptr tgReplyParams = TgBotTranslator::toTgBot(replyParameters);
    TgBot::GenericReply::Ptr tgReplyMarkup = TgBotTranslator::toTgBot(replyMarkup);
    TgBot::Api::ParseMode tgParseMode = convertParseMode(parseMode);
    
    // Call TgBot API
    TgBot::Message::Ptr tgMessage = _bot.getApi().sendMessage(
        chatId, std::string(text), tgReplyParams, tgReplyMarkup, tgParseMode);
    
    // Convert back to project type
    return TgBotTranslator::fromTgBot(tgMessage);
}
```

## Benefits

### 1. Decoupling
- Project code is independent of tgbot-cpp type changes
- Internal type refactoring doesn't affect command modules
- Easier to maintain and update

### 2. API Stability
- TgBotApi interface provides stable contract
- tgbot-cpp updates only require changes in TgBotTranslator
- Reduces breaking changes across the codebase

### 3. Control
- Full control over type definitions
- Can add custom fields/methods without modifying tgbot-cpp
- Message type combines standard fields with parsing capabilities

### 4. Migration Path
- Easier to switch underlying Telegram library if needed
- Translation layer provides abstraction boundary
- Only TgBotApiImpl and TgBotTranslator need changes

### 5. Type Safety
- Strong typing maintained throughout
- Compile-time type checking
- No runtime string-based type conversions

## Migration from MessageExt to Message

The new `tgbot_api::Message` type fully replaces MessageExt:

### What Changed
- **Namespace**: `MessageExt` → `tgbot_api::Message`
- **Location**: `api/MessageExt.hpp` → `api/types/Message.hpp`
- **Structure**: Separate class extending Message → Integrated Message class

### What Stayed the Same
- All template methods: `get<attr>()`, `has<attr>()`, `any()`, `is<attr>()`
- MessageAttrs enum values
- SplitMessageText enum
- internal::message::BotCommand struct
- Parsing functionality via `parseText()`
- Reply message handling

### Command Module Updates Required

Command modules need minimal changes:
```cpp
// Old
#include <api/MessageExt.hpp>
void handler(TgBotApi::Ptr api, MessageExt* message, ...) {
    // Use MessageExt methods
}

// New
#include <api/types/Message.hpp>
void handler(TgBotApi::Ptr api, tgbot_api::Message* message, ...) {
    // Same methods work
}
```

## Remaining Work

### Phase 7: Update Remaining Codebase
1. **TgBotApiImpl**: Implement translation in all `_impl` methods
2. **Socket Components**: Update to use project types
3. **Global Handlers**: Update message handling
4. **Tests**: Update test fixtures and assertions
5. **Lua Bindings**: Update if exposing types to Lua
6. **Remove**: Delete MessageExt.hpp and MessageExt.cpp

### Phase 8: Testing and Validation
1. **Build**: Ensure clean compilation
2. **Unit Tests**: Run and fix tests
3. **Integration Tests**: Verify command modules
4. **Security Review**: Run codeql_checker
5. **Code Review**: Final review of changes

## Files Modified

### Created
- `src/include/api/types/forwards.hpp`
- `src/include/api/types/User.hpp`
- `src/include/api/types/Chat.hpp`
- `src/include/api/types/Message.hpp`
- `src/include/api/types/Media.hpp`
- `src/include/api/types/TelegramTypes.hpp`
- `src/include/api/TgBotTranslator.hpp`
- `src/api/types/Message.cpp`
- `src/api/TgBotTranslator.cpp`

### Modified
- `src/include/api/TgBotApi.hpp`
- `src/include/api/ReplyParametersExt.hpp`
- `src/include/api/Utils.hpp`
- `src/include/api/AuthContext.hpp`
- `src/include/api/CommandModule.hpp`
- `src/api/CMakeLists.txt`

### To Be Removed (Later)
- `src/include/api/MessageExt.hpp`
- `src/api/MessageExt.cpp`

## Notes

1. **TgBot::InlineQueryResult**: Kept as TgBot type for now since it's complex and only used internally
2. **TgBot::EventBroadcaster**: Kept as TgBot type for callback registration
3. **ParseMode Mapping**: Need helper function to convert between enums
4. **StickerFormat Mapping**: Need helper function to convert between enums
5. **Error Handling**: Translation functions return nullptr for null pointers
6. **Performance**: Translations involve copying data, but this is acceptable for the stability gained

## Conclusion

The dependency ejection architecture provides a clean separation between the project and the tgbot-cpp library. The translation layer ensures that tgbot-cpp types are hidden from the rest of the codebase, providing stability and flexibility for future changes. The integration of MessageExt functionality into the Message type simplifies the type system while maintaining all existing capabilities.
