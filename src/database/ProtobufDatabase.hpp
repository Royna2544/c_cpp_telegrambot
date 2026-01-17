#pragma once

#include <TgBotDB.pb.h>
#include <DBImplExports.h>

#include <optional>
#include <ostream>

#include "DatabaseBase.hpp"

using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;
using tgbot::proto::Database;
using tgbot::proto::MediaToName;
using tgbot::proto::PersonList;

struct DBIMPL_EXPORT ProtoDatabase : DatabaseBase {
    [[nodiscard]] ListResult addUserToList(ListType type,
                                           api::types::User::id_type user) const override;
    [[nodiscard]] ListResult removeUserFromList(ListType type,
                                                api::types::User::id_type user) const override;
    [[nodiscard]] ListResult checkUserInList(ListType type,
                                             api::types::User::id_type user) const override;
    bool load(std::filesystem::path filepath) override;
    bool unload() override;
    [[nodiscard]] std::optional<api::types::User::id_type> getOwnerUserId() const override;
    [[nodiscard]] std::optional<MediaInfo> queryMediaInfo(
        std::string str) const override;
    [[nodiscard]] AddResult addMediaInfo(const MediaInfo &info) const override;
    [[nodiscard]] std::vector<MediaInfo> getAllMediaInfos() const override;
    void setOwnerUserId(api::types::User::id_type userId) const override;
    std::ostream &dump(std::ostream &ofs) const override;

    [[nodiscard]] AddResult addChatInfo(
        const api::types::Chat::id_type chatid, const std::string_view name) const override;
    [[nodiscard]] std::optional<api::types::Chat::id_type> getChatId(
        const std::string_view name) const override;

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
    static std::optional<int> findByUid(const RepeatedField<api::types::User::id_type> list,
                                        const api::types::User::id_type uid);
};