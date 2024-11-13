#include "ProtobufDatabase.hpp"

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/strings/match.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <fstream>
#include <optional>

#include "TgBotDB.pb.h"

template <>
struct fmt::formatter<tgbot::proto::MediaType> : formatter<string_view> {
    // parse is inherited from formatter<string_view>.
    auto format(tgbot::proto::MediaType c,
                format_context &ctx) const -> format_context::iterator {
        string_view name = "unknown";
        switch (c) {
            case tgbot::proto::MediaType::VIDEO:
                name = "VIDEO";
                break;
            case tgbot::proto::MediaType::AUDIO:
                name = "AUDIO";
                break;
            case tgbot::proto::MediaType::STICKER:
                name = "STICKER";
                break;
            case tgbot::proto::MediaType::UNKNOWN:
                name = "UNKNOWN";
                break;
            case tgbot::proto::MediaType::PHOTO:
                name = "PHOTO";
                break;
            case tgbot::proto::MediaType::GIF:
                name = "GIF";
                break;
            default:
                LOG(ERROR) << "Unknown media type: " << static_cast<int>(c);
                name = "WTF";
                break;
        }
        return formatter<string_view>::format(name, ctx);
    }
};

using namespace tgbot::proto;

std::optional<int> ProtoDatabase::findByUid(const RepeatedField<UserId> list,
                                            const UserId uid) {
    for (auto it = list.begin(); it != list.end(); ++it) {
        auto distance = std::distance(list.begin(), it);
        if (list.Get(distance) == uid) {
            return distance;
        }
    }
    return std::nullopt;
}

ProtoDatabase::ListResult ProtoDatabase::addUserToList(ListType type,
                                                       UserId user) const {
    auto const otherList = getOtherPersonList(type);
    if (findByUid(otherList.id(), user)) {
        return ListResult::ALREADY_IN_OTHER_LIST;
    }
    auto *const myList = getMutablePersonList(type);
    if (findByUid(myList->id(), user)) {
        return ListResult::ALREADY_IN_LIST;
    }
    myList->add_id(user);
    return ListResult::OK;
}

ProtoDatabase::ListResult ProtoDatabase::removeUserFromList(ListType type,
                                                            UserId user) const {
    auto *const myList = getMutablePersonList(type);
    auto loc = findByUid(myList->id(), user);
    if (loc.has_value()) {
        auto *list = myList->mutable_id();
        list->erase(list->begin() + loc.value());
        return ListResult::OK;
    }
    return ListResult::NOT_IN_LIST;
}

[[nodiscard]] DatabaseBase::ListResult ProtoDatabase::checkUserInList(
    ListType type, UserId user) const {
    auto const myList = getPersonList(type);
    auto loc = findByUid(myList.id(), user);
    if (loc.has_value()) {
        return ListResult::OK;
    }
    auto const otherList = getOtherPersonList(type);
    auto otherLoc = findByUid(myList.id(), user);
    if (otherLoc.has_value()) {
        return ListResult::ALREADY_IN_OTHER_LIST;
    }
    return ListResult::NOT_IN_LIST;
}

bool ProtoDatabase::load(std::filesystem::path filepath) {
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    if (dbinfo.has_value()) {
        LOG(WARNING) << "Database is already loaded";
        return false;
    }

    dbinfo.emplace();
    dbinfo->path = filepath;

    if (filepath == kInMemoryDatabase) {
        LOG(INFO) << "Loading in-memory database";
        return true;
    }

    std::fstream input(filepath.string(), std::ios::in | std::ios::binary);
    if (!input.is_open()) {
        LOG(INFO) << "Creating new file: " << filepath;
        // Nothing to load here...
        return true;
    }
    if (dbinfo->object.ParseFromIstream(&input)) {
        return true;
    }
    LOG(ERROR) << "Failed to parse input file as protobuf";
    dbinfo.reset();
    return false;
}

bool ProtoDatabase::unload() {
    if (!dbinfo.has_value()) {
        LOG(WARNING) << "Database not loaded! Cannot unload!";
        return false;
    }

    if (dbinfo->path == kInMemoryDatabase) {
        // Effectively no-op
        LOG(INFO) << "Unload in-memory database: noop";
        dbinfo.reset();
        return true;
    }

    std::fstream output(dbinfo->path.string(),
                        std::ios::out | std::ios::binary);
    if (!output.is_open()) {
        LOG(ERROR) << "Failed to open output file for writing: "
                   << dbinfo->path;
        return false;
    }
    if (!dbinfo->object.SerializeToOstream(&output)) {
        LOG(ERROR) << "Failed to serialize protobuf to output file";
        return false;
    }
    dbinfo.reset();
    return true;
}

std::optional<UserId> ProtoDatabase::getOwnerUserId() const {
    if (!dbinfo.has_value()) {
        LOG(WARNING) << "Database not loaded! Cannot determine owner user id!";
        return std::nullopt;
    }
    if (!dbinfo->object.has_ownerid()) {
        LOG(WARNING) << "Database does not contain owner user id!";
        return std::nullopt;
    }
    return dbinfo->object.ownerid();
}

void ProtoDatabase::dumpList(std::ostream &os, const PersonList &list,
                             const char *name) {
    const int id_size = list.id_size();
    os << "Dump of " << name << std::endl;
    os << "Size: " << id_size << std::endl;
    if (id_size > 0) {
        for (int i = 0; i < id_size; i++) os << "- " << list.id(i) << std::endl;
        os << std::endl;
    }
}

const PersonList &ProtoDatabase::getPersonList(
    ProtoDatabase::ListType type) const {
    switch (type) {
        case DatabaseBase::ListType::WHITELIST:
            return dbinfo->object.whitelist();
        case DatabaseBase::ListType::BLACKLIST:
            return dbinfo->object.blacklist();
    }
    CHECK(false) << "unreachable";
}

PersonList *ProtoDatabase::getMutablePersonList(ListType type) const {
    switch (type) {
        case DatabaseBase::ListType::WHITELIST:
            return dbinfo->object.mutable_whitelist();
        case DatabaseBase::ListType::BLACKLIST:
            return dbinfo->object.mutable_blacklist();
    }
    CHECK(false) << "unreachable";
}

const PersonList &ProtoDatabase::getOtherPersonList(
    ProtoDatabase::ListType type) const {
    switch (type) {
        case DatabaseBase::ListType::WHITELIST:
            return getPersonList(DatabaseBase::ListType::BLACKLIST);
        case DatabaseBase::ListType::BLACKLIST:
            return getPersonList(DatabaseBase::ListType::WHITELIST);
    }
    CHECK(false) << "unreachable";
}

std::optional<ProtoDatabase::MediaInfo> ProtoDatabase::queryMediaInfo(
    std::string str) const {
    std::optional<tgbot::proto::MediaToName> it;
    const auto &obj = dbinfo->object;
    for (const auto &mediaEntriesIt : obj.mediatonames()) {
        for (const auto &name : mediaEntriesIt.names()) {
            if (absl::EqualsIgnoreCase(name, str)) {
                it = mediaEntriesIt;
                break;
            }
        }
    }
    if (!it.has_value()) {
        return std::nullopt;
    }
    MediaInfo info;
    info.mediaId = it->telegrammediaid();
    info.mediaUniqueId = it->telegrammediauniqueid();
    info.mediaType = static_cast<MediaType>(it->mediatype());
    return info;
}

ProtoDatabase::AddResult ProtoDatabase::addMediaInfo(const MediaInfo &info) const {
    auto *const mediaEntries = dbinfo->object.mutable_mediatonames();
    for (const auto &elem : *mediaEntries) {
        if (elem.telegrammediauniqueid() == info.mediaUniqueId) {
            return AddResult::ALREADY_EXISTS;
        }
    }
    auto *const mediaEntry = mediaEntries->Add();
    mediaEntry->set_telegrammediaid(info.mediaId);
    mediaEntry->set_telegrammediauniqueid(info.mediaUniqueId);
    mediaEntry->set_mediatype(
        static_cast<tgbot::proto::MediaType>(info.mediaType));
    auto *const mediaNames = mediaEntry->mutable_names();
    for (const auto &name : info.names) {
        *mediaNames->Add() = name;
    }
    return AddResult::OK;
}

std::vector<ProtoDatabase::MediaInfo> ProtoDatabase::getAllMediaInfos() const {
    std::vector<MediaInfo> result;
    for (const auto &mediaEntriesIt : dbinfo->object.mediatonames()) {
        MediaInfo info;
        info.mediaId = mediaEntriesIt.telegrammediaid();
        info.mediaUniqueId = mediaEntriesIt.telegrammediauniqueid();
        info.mediaType = static_cast<MediaType>(mediaEntriesIt.mediatype());
        for (const auto &name : mediaEntriesIt.names()) {
            info.names.emplace_back(name);
        }
        result.emplace_back(info);
    }
    return result;
}

std::ostream &ProtoDatabase::dump(std::ostream &os) const {
    if (!dbinfo.has_value()) {
        os << "Database not loaded!";
        return os;
    }
    const auto &db = dbinfo->object;

    os << fmt::format("Dump of database file: {}\nOwner ID: {}\n",
                      dbinfo->path.string(),
                      db.has_ownerid() ? db.ownerid() : -1);

    if (db.has_whitelist()) {
        dumpList(os, db.whitelist(), "whitelist");
    }
    if (db.has_blacklist()) {
        dumpList(os, db.blacklist(), "blacklist");
    }

    int count = 0;
    os << fmt::format("\nMediaNames Dump: (Size {})\n", db.mediatonames_size());
    for (auto const &medias : *getMediaToName()) {
        os << fmt::format("- Entry {}:\n", count++);
        if (medias.has_telegrammediaid()) {
            os << fmt::format("\tMedia FileId: {}\n", medias.telegrammediaid());
        }
        if (medias.has_telegrammediauniqueid()) {
            os << fmt::format("\tMedia Unique FileId: {}\n",
                              medias.telegrammediauniqueid());
        }
        if (medias.has_mediatype()) {
            os << fmt::format("\tMedia type: {}\n", medias.mediatype());
        }
        os << fmt::format("\tMedia Names: {}\n",
                          fmt::join(medias.names(), ", "));
    }
    os << fmt::format("\nChatNames Dump: (Size {})\n",
                      dbinfo->object.chattonames_size());
    count = 0;
    for (const auto &chat : db.chattonames()) {
        os << fmt::format("- Entry {}:\n", count);
        if (chat.has_telegramchatid()) {
            os << fmt::format("\tChat Id: {}\n", chat.telegramchatid());
        }
        if (chat.has_name()) {
            os << fmt::format("\tChat Name: {}\n", chat.name());
        }
    }
    return os;
}

void ProtoDatabase::setOwnerUserId(UserId userId) const {
    if (!dbinfo.has_value()) {
        LOG(WARNING) << "Database not loaded! Cannot set owner user id!";
        return;
    }
    if (dbinfo->object.has_ownerid()) {
        LOG(WARNING) << "Database already contains owner user id!";
        return;
    }
    dbinfo->object.set_ownerid(userId);
}

[[nodiscard]] ProtoDatabase::AddResult ProtoDatabase::addChatInfo(
    const ChatId chatid, const std::string_view name) const {
    auto *const chats = dbinfo->object.mutable_chattonames();
    for (const auto &chat : *chats) {
        if (chat.telegramchatid() == chatid) {
            return AddResult::ALREADY_EXISTS;
        }
    }
    auto *const chat = chats->Add();
    chat->set_telegramchatid(chatid);
    chat->set_name(name.data());
    return AddResult::OK;
}

[[nodiscard]] std::optional<ChatId> ProtoDatabase::getChatId(
    const std::string_view name) const {
    const auto &obj = dbinfo->object;
    for (const auto &chat : obj.chattonames()) {
        if (absl::EqualsIgnoreCase(chat.name(), name)) {
            return chat.telegramchatid();
        }
    }
    return std::nullopt;
}