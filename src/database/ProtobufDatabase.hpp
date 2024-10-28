#pragma once

#include <TgBotDB.pb.h>

#include <TgBotDBImplExports.h>
#include <optional>
#include <ostream>

#include "DatabaseBase.hpp"

using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;
using tgbot::proto::Database;
using tgbot::proto::MediaToName;
using tgbot::proto::PersonList;

struct TgBotDBImpl_API  ProtoDatabase : DatabaseBase {
    [[nodiscard]] ListResult addUserToList(ListType type,
                                           UserId user) const override;
    [[nodiscard]] ListResult removeUserFromList(ListType type,
                                                UserId user) const override;
    [[nodiscard]] ListResult checkUserInList(ListType type,
                                             UserId user) const override;
    bool load(std::filesystem::path filepath) override;
    bool unloadDatabase() override;
    [[nodiscard]] std::optional<UserId> getOwnerUserId() const override;
    [[nodiscard]] std::optional<MediaInfo> queryMediaInfo(
        std::string str) const override;
    [[nodiscard]] bool addMediaInfo(const MediaInfo &info) const override;
    [[nodiscard]] std::vector<MediaInfo> getAllMediaInfos() const override;
    void setOwnerUserId(UserId userId) const override;
    std::ostream &dump(std::ostream &ofs) const override;

    [[nodiscard]] bool addChatInfo(const ChatId chatid,
                                   const std::string &name) const override;
    [[nodiscard]] std::optional<ChatId> getChatId(
        const std::string &name) const override;

   private:
    struct Info {
        mutable Database object;
        std::filesystem::path path;
    };
    std::optional<Info> dbinfo;

    static void dumpList(std::ostream &os, const PersonList &list,
                         const char *name);
    RepeatedPtrField<MediaToName> *getMediaToName() const {
        return dbinfo->object.mutable_mediatonames();
    }
    const PersonList &getPersonList(ListType type) const;
    PersonList *getMutablePersonList(ListType type) const;
    const PersonList &getOtherPersonList(ListType type) const;
    static std::optional<int> findByUid(const RepeatedField<UserId> list,
                                        const UserId uid);
};