#pragma once

#include <gmock/gmock.h>

#include <Random.hpp>
#include <api/TgBotApi.hpp>
#include <database/DatabaseBase.hpp>

#include "ResourceManager.h"
#include "StringResLoader.hpp"
#include "api/Providers.hpp"
#include "bot/TgBotDatabaseImpl.hpp"
#include "fruit/fruit.h"
#include "fruit/fruit_forward_decls.h"
#include "fruit/provider.h"
#include "trivial_helpers/fruit_inject.hpp"

class MockDatabase : public DatabaseBase {
   public:
    APPLE_INJECT(MockDatabase()) = default;

    MOCK_METHOD(DatabaseBase::ListResult, addUserToList,
                (DatabaseBase::ListType type, UserId user), (const, override));

    MOCK_METHOD(DatabaseBase::ListResult, removeUserFromList,
                (DatabaseBase::ListType type, UserId user), (const, override));

    MOCK_METHOD(DatabaseBase::ListResult, checkUserInList,
                (DatabaseBase::ListType type, UserId user), (const, override));

    MOCK_METHOD(std::optional<UserId>, getOwnerUserId, (), (const, override));

    MOCK_METHOD(std::optional<DatabaseBase::MediaInfo>, queryMediaInfo,
                (std::string str), (const, override));

    MOCK_METHOD(bool, addMediaInfo, (const DatabaseBase::MediaInfo& info),
                (const, override));

    MOCK_METHOD(std::vector<MediaInfo>, getAllMediaInfos, (), (const override));

    MOCK_METHOD(std::ostream&, dump, (std::ostream & ofs), (const, override));

    MOCK_METHOD(void, setOwnerUserId, (UserId userid), (const, override));

    MOCK_METHOD(bool, addChatInfo,
                (const ChatId chatid, const std::string& name),
                (const, override));

    MOCK_METHOD(std::optional<ChatId>, getChatId, (const std::string& name),
                (const, override));

    MOCK_METHOD(bool, load, (std::filesystem::path filepath), (override));
    MOCK_METHOD(bool, unloadDatabase, (), (override));
};

class MockTgBotApi : public TgBotApi {
   public:
    APPLE_INJECT(MockTgBotApi()) = default;

    MOCK_METHOD(Message::Ptr, sendMessage_impl,
                (ChatId chatId, const std::string_view text,
                 ReplyParametersExt::Ptr replyParameters,
                 GenericReply::Ptr replyMarkup,
                 const std::string_view parseMode),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendAnimation_impl,
                (ChatId chatId, FileOrString animation,
                 const std::string_view caption,
                 ReplyParametersExt::Ptr replyParameters,
                 GenericReply::Ptr replyMarkup,
                 const std::string_view parseMode),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendSticker_impl,
                (ChatId chatId, FileOrString sticker,
                 ReplyParametersExt::Ptr replyParameters),
                (const, override));

    MOCK_METHOD(Message::Ptr, editMessage_impl,
                (const Message::Ptr& message, const std::string_view newText,
                 const TgBot::InlineKeyboardMarkup::Ptr& markup,
                 const std::string_view parseMode),
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
                 std::uint32_t untilDate),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendDocument_impl,
                (ChatId chatId, FileOrString document,
                 const std::string_view caption,
                 ReplyParametersExt::Ptr replyParameters,
                 GenericReply::Ptr replyMarkup,
                 const std::string_view parseMode),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendPhoto_impl,
                (ChatId chatId, FileOrString photo,
                 const std::string_view caption,
                 ReplyParametersExt::Ptr replyParameters,
                 GenericReply::Ptr replyMarkup,
                 const std::string_view parseMode),
                (const, override));

    MOCK_METHOD(Message::Ptr, sendVideo_impl,
                (ChatId chatId, FileOrString photo,
                 const std::string_view caption,
                 ReplyParametersExt::Ptr replyParameters,
                 GenericReply::Ptr replyMarkup,
                 const std::string_view parseMode),
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
                 const std::string_view stickerFormat),
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

    // Non-TgBotApi methods
    MOCK_METHOD(bool, reloadCommand, (const std::string_view cmd), (override));
    MOCK_METHOD(bool, unloadCommand, (const std::string_view cmd), (override));
    MOCK_METHOD(void, onAnyMessage, (const AnyMessageCallback& callback),
                (override));
};

class MockRandom : public Random::ImplBase {
   public:
    using ret_type = Random::ret_type;
    APPLE_INJECT(MockRandom()) = default;

    MOCK_METHOD(bool, isSupported, (), (const, override));
    MOCK_METHOD(ret_type, generate, (const ret_type min, const ret_type max),
                (const, override));
    MOCK_METHOD(std::string_view, getName, (), (const));
    MOCK_METHOD(void, shuffle, (std::vector<std::string>&), (const));
};

class MockResource : public ResourceProvider {
   public:
    APPLE_INJECT(MockResource()) = default;

    MOCK_METHOD(std::string_view, get, (std::filesystem::path filename),
                (const, override));
    MOCK_METHOD(bool, preload, (std::filesystem::path p), ());
};

struct MockLocaleStrings : public StringResLoaderBase::LocaleStrings {
   public:
    APPLE_INJECT(MockLocaleStrings()) = default;

    MOCK_METHOD(std::string_view, get, (const Strings& string),
                (const, override));
    MOCK_METHOD(size_t, size, (), (override, const, noexcept));
};

inline fruit::Component<Providers> getProviders() {
    return fruit::createComponent()
        .bind<Random::ImplBase, MockRandom>()
        .bind<ResourceProvider, MockResource>()
        .bind<DatabaseBase, MockDatabase>();
}
