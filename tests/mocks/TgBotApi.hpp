#pragma once

#include <gmock/gmock.h>

#include <api/TgBotApi.hpp>

class MockTgBotApi : public TgBotApi {
   public:
    APPLE_INJECT(MockTgBotApi()) = default;

    MOCK_METHOD(Message::Ptr, sendMessage_impl,
                (ChatId chatId, const std::string_view text,
                 ReplyParametersExt::Ptr replyParameters,
                 GenericReply::Ptr replyMarkup, const ParseMode parseMode),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendAnimation_impl,
                (ChatId chatId, FileOrString animation,
                 const std::string_view caption,
                 ReplyParametersExt::Ptr replyParameters,
                 GenericReply::Ptr replyMarkup, const ParseMode parseMode),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendSticker_impl,
                (ChatId chatId, FileOrString sticker,
                 ReplyParametersExt::Ptr replyParameters),
                (const, override));

    MOCK_METHOD(Message::Ptr, editMessage_impl,
                (const Message::Ptr& message, const std::string_view newText,
                 const TgBot::InlineKeyboardMarkup::Ptr& markup,
                 const ParseMode parseMode),
                (const, override));

    MOCK_METHOD(Message::Ptr, editMessageMarkup_impl,
                (const StringOrMessage& message,
                 const GenericReply::Ptr& replyMarkup),
                (const, override));

    MOCK_METHOD(void, deleteMessage_impl, (const Message::Ptr& message),
                (const, override));

    MOCK_METHOD(void, deleteMessages_impl,
                (ChatId chatId, const std::vector<MessageId>& messageIds),
                (const, override));

    MOCK_METHOD(void, restrictChatMember_impl,
                (ChatId chatId, UserId userId,
                 TgBot::ChatPermissions::Ptr permissions,
                 std::chrono::system_clock::time_point untilDate),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendDocument_impl,
                (ChatId chatId, FileOrString document,
                 const std::string_view caption,
                 ReplyParametersExt::Ptr replyParameters,
                 GenericReply::Ptr replyMarkup, const ParseMode parseMode),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendPhoto_impl,
                (ChatId chatId, FileOrString photo,
                 const std::string_view caption,
                 ReplyParametersExt::Ptr replyParameters,
                 GenericReply::Ptr replyMarkup, const ParseMode parseMode),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendVideo_impl,
                (ChatId chatId, FileOrString photo,
                 const std::string_view caption,
                 ReplyParametersExt::Ptr replyParameters,
                 GenericReply::Ptr replyMarkup, const ParseMode parseMode),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendDice_impl, (const ChatId chatId),
                (const, override));

    MOCK_METHOD(StickerSet::Ptr, getStickerSet_impl,
                (const std::string_view setName), (const, override));

    MOCK_METHOD(bool, createNewStickerSet_impl,
                (std::int64_t userId, const std::string_view name,
                 const std::string_view title,
                 const std::vector<InputSticker::Ptr>& stickers,
                 Sticker::Type stickerType),
                (const, override));

    MOCK_METHOD(File::Ptr, uploadStickerFile_impl,
                (std::int64_t userId, InputFile::Ptr sticker,
                 const TgBot::Api::StickerFormat stickerFormat),
                (const, override));

    MOCK_METHOD(bool, downloadFile_impl,
                (const std::filesystem::path& destfilename,
                 const std::string_view fileid),
                (const, override));

    MOCK_METHOD(User::Ptr, getBotUser_impl, (), (const, override));

    MOCK_METHOD(MessageId, copyMessage_impl,
                (ChatId chat, MessageId message, ReplyParametersExt::Ptr reply),
                (const, override));
    MOCK_METHOD(bool, answerCallbackQuery_impl,
                (const std::string_view callbackQueryId,
                 const std::string_view text, bool showAlert),
                (const override));

    MOCK_METHOD(bool, pinMessage_impl, (Message::Ptr message),
                (const override));
    MOCK_METHOD(bool, unpinMessage_impl, (Message::Ptr message),
                (const override));
    MOCK_METHOD(bool, banChatMember_impl,
                (const Chat::Ptr& chat, const User::Ptr& user),
                (const override));
    MOCK_METHOD(bool, unbanChatMember_impl,
                (const Chat::Ptr& chat, const User::Ptr& user),
                (const override));
    MOCK_METHOD(User::Ptr, getChatMember_impl, (const ChatId, const UserId),
                (const override));
    MOCK_METHOD(void, setDescriptions_impl,
                (const std::string_view description,
                 const std::string_view shortDescription),
                (const override));
    MOCK_METHOD(bool, setMessageReaction_impl,
                (const ChatId chatid, const MessageId message,
                 const std::vector<ReactionType::Ptr>& reaction, bool isBig),
                (const override));

    // Non-TgBotApi methods
    MOCK_METHOD(bool, reloadCommand, (const std::string& cmd), (override));
    MOCK_METHOD(bool, unloadCommand, (const std::string& cmd), (override));
    MOCK_METHOD(void, onAnyMessage, (const AnyMessageCallback& callback),
                (override));
};
