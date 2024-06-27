#pragma once

#include <TgBotDB.pb.h>

#include <optional>
#include <ostream>

#include "DatabaseBase.hpp"

using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;
using tgbot::proto::Database;
using tgbot::proto::MediaToName;
using tgbot::proto::PersonList;

struct ProtoDatabase : DatabaseBase {
    [[nodiscard]] ListResult addUserToList(ListType type,
                                           UserId user) const override;
    [[nodiscard]] ListResult removeUserFromList(ListType type,
                                                UserId user) const override;
    [[nodiscard]] ListResult checkUserInList(ListType type,
                                             UserId user) const override;
    bool loadDatabaseFromFile(std::filesystem::path filepath) override;
    bool unloadDatabase() override;
    UserId getOwnerUserId() const override;
    std::optional<MediaInfo> queryMediaInfo(std::string str) const override;
    bool addMediaInfo(const MediaInfo &info) const override;
    void setOwnerUserId(UserId userId) const override;
    std::ostream &dump(std::ostream &ofs) const override;

   private:
    struct Info {
        mutable Database protoDatabaseObject;
        std::filesystem::path protoFilePath;
    };
    std::optional<Info> db_info;

    static void dumpList(std::ostream &os, const PersonList &list,
                         const char *name);
    RepeatedPtrField<MediaToName> *getMediaToName() const {
        return db_info->protoDatabaseObject.mutable_mediatonames();
    }
    const PersonList &getPersonList(ListType type) const;
    PersonList *getMutablePersonList(ListType type) const;
    const PersonList &getOtherPersonList(ListType type) const;
    static std::optional<int> findByUid(const RepeatedField<UserId> list,
                                        const UserId uid);
};