#pragma once

#include <gmock/gmock.h>

#include <database/DatabaseBase.hpp>
#include <trivial_helpers/fruit_inject.hpp>

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

    MOCK_METHOD(MockDatabase::AddResult, addMediaInfo,
                (const DatabaseBase::MediaInfo& info), (const, override));

    MOCK_METHOD(std::vector<MediaInfo>, getAllMediaInfos, (), (const override));

    MOCK_METHOD(std::ostream&, dump, (std::ostream & ofs), (const, override));

    MOCK_METHOD(void, setOwnerUserId, (UserId userid), (const, override));

    MOCK_METHOD(MockDatabase::AddResult, addChatInfo,
                (const ChatId chatid, const std::string_view name),
                (const, override));

    MOCK_METHOD(std::optional<ChatId>, getChatId, (const std::string_view name),
                (const, override));

    MOCK_METHOD(std::vector<ChatInfo>, getAllChatInfos, (), (const override));

    MOCK_METHOD(bool, deleteMediaInfo,
                (const decltype(MediaInfo::mediaId) mediaId),
                (const, override));
    MOCK_METHOD(std::optional<std::string>, getChatName, (const ChatId chatId),
                (const, override));
    MOCK_METHOD(std::optional<std::vector<decltype(MediaInfo::mediaId)>>,
                getMediaIds, (const std::string_view alias), (const, override));
    MOCK_METHOD(bool, deleteChatInfo, (const ChatId chatId), (const, override));
    MOCK_METHOD(bool, load, (std::filesystem::path filepath), (override));
    MOCK_METHOD(bool, unload, (), (override));
};
