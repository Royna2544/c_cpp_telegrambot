#include "ProtobufDatabase.hpp"

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <fstream>
#include <optional>

#include "TgBotDB.pb.h"

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

    std::fstream input(filepath.string(), std::ios::in | std::ios::binary);
    if (!input.is_open()) {
        LOG(INFO) << "Creating new file";
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

bool ProtoDatabase::unloadDatabase() {
    if (!dbinfo.has_value()) {
        LOG(WARNING) << "Database not loaded! Cannot unload!";
        return false;
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
            if (strcasecmp(name.c_str(), str.c_str()) == 0) {
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
    return info;
}

bool ProtoDatabase::addMediaInfo(const MediaInfo &info) const {
    auto *const mediaEntries = dbinfo->object.mutable_mediatonames();
    for (const auto &elem : *mediaEntries) {
        if (elem.telegrammediauniqueid() == info.mediaUniqueId) {
            return false;
        }
    }
    auto *const mediaEntry = mediaEntries->Add();
    mediaEntry->set_telegrammediaid(info.mediaId);
    mediaEntry->set_telegrammediauniqueid(info.mediaUniqueId);
    auto *const mediaNames = mediaEntry->mutable_names();
    for (const auto &name : info.names) {
        *mediaNames->Add() = name;
    }
    return true;
}

std::ostream &ProtoDatabase::dump(std::ostream &os) const {
    if (!dbinfo.has_value()) {
        os << "Database not loaded!";
        return os;
    }
    const auto &db = dbinfo->object;
    os << "Dump of database file: " << dbinfo->path << std::endl;
    os << "Owner ID: ";
    if (db.has_ownerid()) {
        os << db.ownerid();
    } else {
        os << "Not set";
    }
    os << std::endl;

    if (db.has_whitelist()) {
        dumpList(os, db.whitelist(), "whitelist");
    }
    if (db.has_blacklist()) {
        dumpList(os, db.blacklist(), "blacklist");
    }
    const auto &mediaDB = getMediaToName();
    if (const auto mediaDBSize = mediaDB->size(); mediaDBSize > 0) {
        for (int i = 0; i < mediaDBSize; ++i) {
            const auto it = mediaDB->Get(i);
            std::cout << "- Entry " << i << ":" << std::endl;
            if (it.has_telegrammediaid()) {
                std::cout << "   -> "
                             "Media FileId: "
                          << it.telegrammediaid() << std::endl;
            }
            if (it.has_telegrammediauniqueid()) {
                std::cout << "   -> "
                             "Media FileUniqueId: "
                          << it.telegrammediauniqueid() << std::endl;
            }
            if (const auto namesSize = it.names_size(); namesSize > 0) {
                for (int j = 0; j < namesSize; ++j) {
                    std::cout << "   -> "
                                 "Media Name "
                              << j << ": " << it.names(j) << std::endl;
                }
            }
            if (i != mediaDBSize - 1) {
                std::cout << std::endl;
            }
        }
    }
    const auto chatDB = dbinfo->object.chattonames();
    if (const auto chatDBSize = chatDB.size(); chatDBSize > 0) {
        for (int i = 0; i < chatDBSize; ++i) {
            const auto it = chatDB.Get(i);
            std::cout << "- Entry " << i << ":" << std::endl;
            if (it.has_telegramchatid()) {
                std::cout << "   -> "
                             "Chat Id: "
                          << it.telegramchatid() << std::endl;
            }
            if (it.has_name()) {
                std::cout << "   -> "
                             "Chat Name: "
                          << it.name() << std::endl;
            }
            if (i != chatDBSize - 1) {
                std::cout << std::endl;
            }
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

[[nodiscard]] bool ProtoDatabase::addChatInfo(const ChatId chatid,
                                              const std::string &name) const {
    auto *const chats = dbinfo->object.mutable_chattonames();
    for (const auto &chat : *chats) {
        if (chat.telegramchatid() == chatid) {
            return false;
        }
    }
    auto *const chat = chats->Add();
    chat->set_telegramchatid(chatid);
    chat->set_name(name);
    return true;
}

[[nodiscard]] std::optional<ChatId> ProtoDatabase::getChatId(
    const std::string &name) const {
    const auto &obj = dbinfo->object;
    for (const auto &chat : obj.chattonames()) {
        if (strcasecmp(chat.name().c_str(), name.c_str()) == 0) {
            return chat.telegramchatid();
        }
    }
    return std::nullopt;
}