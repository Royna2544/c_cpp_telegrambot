#pragma once

#include <TgBotDB.pb.h>

#include <optional>

#include "DatabaseBase.hpp"

using tgbot::proto::MediaToName;
using tgbot::proto::PersonList;
using tgbot::proto::Database;
using google::protobuf::RepeatedField;
using google::protobuf::RepeatedPtrField;

struct ProtoDatabase : DatabaseBase {
    [[nodiscard]] ListResult addUserToList(ListType type, UserId user) override;
    [[nodiscard]] ListResult removeUserFromList(ListType type,
                                                UserId user) override;
    [[nodiscard]] ListResult checkUserInList(ListType type,
                                             UserId user) const override;
    bool loadDatabaseFromFile(std::filesystem::path filepath) override;
    bool unloadDatabase() override;
    UserId getOwnerUserId() const override;
    std::optional<MediaInfo> queryMediaInfo(std::string str) const override;
    bool addMediaInfo(const MediaInfo &info) const override;
    friend std::ostream &operator<<(std::ostream &os, ProtoDatabase protoDB);

    RepeatedPtrField<MediaToName> *getMediaToName() {
        return db_info->protoDatabaseObject.mutable_mediatonames();
    }
    static void dumpList(std::ostream &os, const PersonList &list,
                         const char *name);

   private:
    struct Info {
        mutable Database protoDatabaseObject;
        std::filesystem::path protoFilePath;
    };
    std::optional<Info> db_info;

    const PersonList &getPersonList(ListType type) const;
    PersonList *getMutablePersonList(ListType type);
    const PersonList &getOtherPersonList(ListType type) const;
    static std::optional<int> findByUid(const RepeatedField<UserId> list,
                                        const UserId uid);
};