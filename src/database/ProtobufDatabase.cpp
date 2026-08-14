#include "ProtobufDatabase.hpp"

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/strings/match.h>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <atomic>
#include <fstream>
#include <optional>
#include <trivial_helpers/log_once.hpp>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include "TgBotDB.pb.h"

template <>
struct fmt::formatter<glider::proto::database::MediaType>
    : formatter<string_view> {
    // parse is inherited from formatter<string_view>.
    auto format(glider::proto::database::MediaType c, format_context& ctx) const
        -> format_context::iterator {
        string_view name = "unknown";
        switch (c) {
            case glider::proto::database::MediaType::VIDEO:
                name = "VIDEO";
                break;
            case glider::proto::database::MediaType::AUDIO:
                name = "AUDIO";
                break;
            case glider::proto::database::MediaType::STICKER:
                name = "STICKER";
                break;
            case glider::proto::database::MediaType::UNKNOWN:
                name = "UNKNOWN";
                break;
            case glider::proto::database::MediaType::PHOTO:
                name = "PHOTO";
                break;
            case glider::proto::database::MediaType::GIF:
                name = "GIF";
                break;
            case glider::proto::database::MediaType::DOCUMENT:
                name = "DOCUMENT";
                break;
            default:
                LOG(ERROR) << "Unknown media type: " << static_cast<int>(c);
                name = "WTF";
                break;
        }
        return formatter<string_view>::format(name, ctx);
    }
};

using namespace glider::proto::database;

namespace {

std::filesystem::path temporarySnapshotPath(
    const std::filesystem::path& target) {
    static std::atomic_uint64_t sequence{0};
    auto temporary = target;
#ifdef _WIN32
    temporary +=
        fmt::format(".tmp.{}.{}", GetCurrentProcessId(), sequence.fetch_add(1));
#else
    temporary += fmt::format(".tmp.{}.{}", getpid(), sequence.fetch_add(1));
#endif
    return temporary;
}

bool syncSnapshotFile(const std::filesystem::path& path) {
#ifdef _WIN32
    const HANDLE file =
        CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return false;
    }
    const bool ok = FlushFileBuffers(file) != 0;
    CloseHandle(file);
    return ok;
#else
    const int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return false;
    }
    const bool ok = fsync(fd) == 0;
    close(fd);
    return ok;
#endif
}

bool replaceSnapshotFile(const std::filesystem::path& source,
                         const std::filesystem::path& target) {
#ifdef _WIN32
    std::error_code existsError;
    const bool targetExists = std::filesystem::exists(target, existsError);
    if (existsError)
        return false;
    if (targetExists) {
        return ReplaceFileW(target.c_str(), source.c_str(), nullptr,
                            REPLACEFILE_WRITE_THROUGH, nullptr, nullptr) != 0;
    }
    return MoveFileExW(source.c_str(), target.c_str(),
                       MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code ec;
    std::filesystem::rename(source, target, ec);
    if (ec) {
        return false;
    }

    auto directory = target.parent_path();
    if (directory.empty()) {
        directory = ".";
    }
    const int fd = open(directory.c_str(), O_RDONLY | O_DIRECTORY);
    if (fd >= 0) {
        if (fsync(fd) != 0) {
            LOG(WARNING) << "Failed to fsync protobuf database directory "
                         << directory;
        }
        close(fd);
    } else {
        LOG(WARNING) << "Failed to open protobuf database directory for fsync: "
                     << directory;
    }
    // The atomic rename already succeeded. A directory-fsync failure weakens
    // crash guarantees but must not roll back memory after the new snapshot is
    // visible at the target path.
    return true;
#endif
}

}  // namespace

ProtoDatabase::~ProtoDatabase() = default;

bool ProtoDatabase::persistLocked() const {
    if (!dbinfo) {
        return false;
    }
    if (dbinfo->path == kInMemoryDatabase) {
        return true;
    }

    const auto temporary = temporarySnapshotPath(dbinfo->path);
    std::error_code ec;
    std::filesystem::remove(temporary, ec);

    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output.is_open() || !dbinfo->object.SerializeToOstream(&output)) {
        LOG(ERROR) << "Failed to serialize protobuf snapshot to " << temporary;
        output.close();
        std::filesystem::remove(temporary, ec);
        return false;
    }
    output.flush();
    if (!output.good()) {
        LOG(ERROR) << "Failed to flush protobuf snapshot " << temporary;
        output.close();
        std::filesystem::remove(temporary, ec);
        return false;
    }
    output.close();
    if (!output.good()) {
        LOG(ERROR) << "Failed to close protobuf snapshot " << temporary;
        std::filesystem::remove(temporary, ec);
        return false;
    }

    if (!syncSnapshotFile(temporary) ||
        !replaceSnapshotFile(temporary, dbinfo->path)) {
        LOG(ERROR) << "Failed to atomically replace protobuf database "
                   << dbinfo->path;
        std::filesystem::remove(temporary, ec);
        return false;
    }
    return true;
}

std::optional<int> ProtoDatabase::findByUid(const RepeatedField<UserId>& list,
                                            const UserId uid) {
    for (int index = 0; index < list.size(); ++index) {
        if (list.Get(index) == uid) {
            return index;
        }
    }
    return std::nullopt;
}

ProtoDatabase::ListResult ProtoDatabase::addUserToList(ListType type,
                                                       UserId user) const {
    std::lock_guard<std::mutex> lock(dbinfo_mutex_);
    if (!dbinfo) {
        return ListResult::BACKEND_ERROR;
    }
    if (type == ListType::BLACKLIST && dbinfo->object.has_ownerid() &&
        dbinfo->object.ownerid() == user) {
        return ListResult::ALREADY_IN_OTHER_LIST;
    }
    const auto& otherList = getOtherPersonList(type);
    if (findByUid(otherList.id(), user)) {
        return ListResult::ALREADY_IN_OTHER_LIST;
    }
    auto* const myList = getMutablePersonList(type);
    if (findByUid(myList->id(), user)) {
        return ListResult::ALREADY_IN_LIST;
    }
    Database previous = dbinfo->object;
    myList->add_id(user);
    if (persistLocked()) {
        return ListResult::OK;
    }
    dbinfo->object = std::move(previous);
    return ListResult::BACKEND_ERROR;
}

ProtoDatabase::ListResult ProtoDatabase::removeUserFromList(ListType type,
                                                            UserId user) const {
    std::lock_guard<std::mutex> lock(dbinfo_mutex_);
    if (!dbinfo) {
        return ListResult::BACKEND_ERROR;
    }
    auto* const myList = getMutablePersonList(type);
    auto loc = findByUid(myList->id(), user);
    if (loc.has_value()) {
        Database previous = dbinfo->object;
        auto* list = myList->mutable_id();
        list->erase(list->begin() + loc.value());
        if (persistLocked()) {
            return ListResult::OK;
        }
        dbinfo->object = std::move(previous);
        return ListResult::BACKEND_ERROR;
    }
    return ListResult::NOT_IN_LIST;
}

[[nodiscard]] DatabaseBase::ListResult ProtoDatabase::checkUserInList(
    ListType type, UserId user) const {
    std::lock_guard<std::mutex> lock(dbinfo_mutex_);
    const auto& myList = getPersonList(type);
    auto loc = findByUid(myList.id(), user);
    if (loc.has_value()) {
        return ListResult::OK;
    }
    const auto& otherList = getOtherPersonList(type);
    auto otherLoc = findByUid(otherList.id(), user);
    if (otherLoc.has_value()) {
        return ListResult::ALREADY_IN_OTHER_LIST;
    }
    return ListResult::NOT_IN_LIST;
}

std::optional<std::string> ProtoDatabase::getChatName(
    const ChatId chatId) const {
    std::lock_guard<std::mutex> lock(dbinfo_mutex_);
    if (!dbinfo.has_value()) {
        LOG_ONCE(WARNING) << "Database not loaded! Cannot determine chat name!";
        return std::nullopt;
    }
    for (const auto& chatInfo : dbinfo->object.chattonames()) {
        if (chatInfo.telegramchatid() == chatId)
            return chatInfo.name();
    }
    return std::nullopt;
}

bool ProtoDatabase::deleteChatInfo(const ChatId chatId) const {
    std::lock_guard<std::mutex> lock(dbinfo_mutex_);
    if (!dbinfo.has_value()) {
        LOG_ONCE(WARNING) << "Database not loaded! Cannot delete chat info!";
        return false;
    }
    auto* chatToNames = dbinfo->object.mutable_chattonames();
    for (int i = 0; i < chatToNames->size(); ++i) {
        if (chatToNames->Get(i).telegramchatid() == chatId) {
            Database previous = dbinfo->object;
            chatToNames->DeleteSubrange(i, 1);
            if (persistLocked()) {
                return true;
            }
            dbinfo->object = std::move(previous);
            return false;
        }
    }
    return false;
}

std::vector<ProtoDatabase::ChatInfo> ProtoDatabase::getAllChatInfos() const {
    std::lock_guard<std::mutex> lock(dbinfo_mutex_);
    if (!dbinfo.has_value()) {
        LOG_ONCE(WARNING) << "Database not loaded! Cannot get chat infos!";
        return {};
    }
    std::vector<ChatInfo> result;
    for (const auto& chatInfo : dbinfo->object.chattonames()) {
        ChatInfo info;
        info.chatId = chatInfo.telegramchatid();
        info.name = chatInfo.name();
        result.emplace_back(std::move(info));
    }
    return result;
}

bool ProtoDatabase::load(std::filesystem::path filepath) {
    std::lock_guard<std::mutex> lock(dbinfo_mutex_);
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
    std::lock_guard<std::mutex> lock(dbinfo_mutex_);
    if (!dbinfo.has_value()) {
        LOG(WARNING) << "Database not loaded! Cannot unload!";
        return false;
    }

    if (!persistLocked()) {
        return false;
    }
    dbinfo.reset();
    return true;
}

std::optional<UserId> ProtoDatabase::getOwnerUserId() const {
    std::lock_guard<std::mutex> lock(dbinfo_mutex_);
    if (!dbinfo.has_value()) {
        LOG_ONCE(WARNING)
            << "Database not loaded! Cannot determine owner user id!";
        return std::nullopt;
    }
    if (!dbinfo->object.has_ownerid()) {
        LOG_ONCE(WARNING) << "Database does not contain owner user id!";
        return std::nullopt;
    }
    return dbinfo->object.ownerid();
}

void ProtoDatabase::dumpList(std::ostream& os, const PersonList& list,
                             const char* name) {
    os << fmt::format("Dump of {}\nSize: {}\n{}", name, list.id_size(),
                      fmt::join(list.id(), "\n"));
}

const PersonList& ProtoDatabase::getPersonList(
    ProtoDatabase::ListType type) const {
    switch (type) {
        case DatabaseBase::ListType::WHITELIST:
            return dbinfo->object.whitelist();
        case DatabaseBase::ListType::BLACKLIST:
            return dbinfo->object.blacklist();
    }
    CHECK(false) << "unreachable";
}

PersonList* ProtoDatabase::getMutablePersonList(ListType type) const {
    switch (type) {
        case DatabaseBase::ListType::WHITELIST:
            return dbinfo->object.mutable_whitelist();
        case DatabaseBase::ListType::BLACKLIST:
            return dbinfo->object.mutable_blacklist();
    }
    CHECK(false) << "unreachable";
}

const PersonList& ProtoDatabase::getOtherPersonList(
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
    std::lock_guard<std::mutex> lock(dbinfo_mutex_);
    std::optional<glider::proto::database::MediaToName> it;
    const auto& obj = dbinfo->object;
    for (const auto& mediaEntriesIt : obj.mediatonames()) {
        for (const auto& name : mediaEntriesIt.names()) {
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

ProtoDatabase::AddResult ProtoDatabase::addMediaInfo(
    const MediaInfo& info) const {
    std::lock_guard<std::mutex> lock(dbinfo_mutex_);
    if (!dbinfo) {
        return AddResult::BACKEND_ERROR;
    }
    auto* const mediaEntries = dbinfo->object.mutable_mediatonames();
    for (const auto& elem : *mediaEntries) {
        if (elem.telegrammediauniqueid() == info.mediaUniqueId) {
            return AddResult::ALREADY_EXISTS;
        }
    }
    Database previous = dbinfo->object;
    auto* const mediaEntry = mediaEntries->Add();
    mediaEntry->set_telegrammediaid(info.mediaId);
    mediaEntry->set_telegrammediauniqueid(info.mediaUniqueId);
    mediaEntry->set_mediatype(
        static_cast<glider::proto::database::MediaType>(info.mediaType));
    auto* const mediaNames = mediaEntry->mutable_names();
    for (const auto& name : info.names) {
        *mediaNames->Add() = name;
    }
    if (persistLocked()) {
        return AddResult::OK;
    }
    dbinfo->object = std::move(previous);
    return AddResult::BACKEND_ERROR;
}

std::vector<ProtoDatabase::MediaInfo> ProtoDatabase::getAllMediaInfos() const {
    std::lock_guard<std::mutex> lock(dbinfo_mutex_);
    std::vector<MediaInfo> result;
    for (const auto& mediaEntriesIt : dbinfo->object.mediatonames()) {
        MediaInfo info;
        info.mediaId = mediaEntriesIt.telegrammediaid();
        info.mediaUniqueId = mediaEntriesIt.telegrammediauniqueid();
        info.mediaType = static_cast<MediaType>(mediaEntriesIt.mediatype());
        for (const auto& name : mediaEntriesIt.names()) {
            info.names.emplace_back(name);
        }
        result.emplace_back(info);
    }
    return result;
}

bool ProtoDatabase::deleteMediaInfo(
    const decltype(MediaInfo::mediaId) mediaId) const {
    std::lock_guard<std::mutex> lock(dbinfo_mutex_);
    auto* mediaToNames = dbinfo->object.mutable_mediatonames();
    for (int i = 0; i < mediaToNames->size(); ++i) {
        if (mediaToNames->Get(i).telegrammediaid() == mediaId) {
            Database previous = dbinfo->object;
            mediaToNames->DeleteSubrange(i, 1);
            if (persistLocked()) {
                return true;
            }
            dbinfo->object = std::move(previous);
            return false;
        }
    }
    return false;
}

std::optional<std::vector<decltype(ProtoDatabase::MediaInfo::mediaId)>>
ProtoDatabase::getMediaIds(const std::string_view alias) const {
    std::lock_guard<std::mutex> lock(dbinfo_mutex_);
    std::vector<decltype(MediaInfo::mediaId)> result;
    const auto& obj = dbinfo->object;
    for (const auto& mediaEntriesIt : obj.mediatonames()) {
        for (const auto& name : mediaEntriesIt.names()) {
            if (absl::EqualsIgnoreCase(name, alias)) {
                result.emplace_back(mediaEntriesIt.telegrammediaid());
            }
        }
    }
    if (result.empty()) {
        return std::nullopt;
    }
    return result;
    ;
}

std::ostream& ProtoDatabase::dump(std::ostream& os) const {
    std::lock_guard<std::mutex> lock(dbinfo_mutex_);
    if (!dbinfo.has_value()) {
        os << "Database not loaded!";
        return os;
    }
    const auto& db = dbinfo->object;

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
    for (auto const& medias : *getMediaToName()) {
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
    for (const auto& chat : db.chattonames()) {
        os << fmt::format("- Entry {}:\n", count++);
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
    (void)claimOwnerUserId(userId);
}

DatabaseBase::OwnerClaimResult ProtoDatabase::claimOwnerUserId(
    const UserId userId) const {
    std::lock_guard<std::mutex> lock(dbinfo_mutex_);
    if (!dbinfo.has_value()) {
        LOG(WARNING) << "Database not loaded! Cannot set owner user id!";
        return OwnerClaimResult::BACKEND_ERROR;
    }
    if (dbinfo->object.has_ownerid()) {
        LOG(WARNING) << "Database already contains owner user id!";
        return OwnerClaimResult::ALREADY_SET;
    }
    if (findByUid(dbinfo->object.blacklist().id(), userId)) {
        LOG(ERROR) << "Refusing to claim blacklisted user " << userId
                   << " as owner";
        return OwnerClaimResult::BACKEND_ERROR;
    }
    dbinfo->object.set_ownerid(userId);
    if (persistLocked()) {
        return OwnerClaimResult::OK;
    }
    dbinfo->object.clear_ownerid();
    return OwnerClaimResult::BACKEND_ERROR;
}

[[nodiscard]] ProtoDatabase::AddResult ProtoDatabase::addChatInfo(
    const ChatId chatid, const std::string_view name) const {
    std::lock_guard<std::mutex> lock(dbinfo_mutex_);
    if (!dbinfo) {
        return AddResult::BACKEND_ERROR;
    }
    auto* const chats = dbinfo->object.mutable_chattonames();
    for (const auto& chat : *chats) {
        if (chat.telegramchatid() == chatid) {
            return AddResult::ALREADY_EXISTS;
        }
    }
    Database previous = dbinfo->object;
    auto* const chat = chats->Add();
    chat->set_telegramchatid(chatid);
    chat->set_name(name.data(), name.size());
    if (persistLocked()) {
        return AddResult::OK;
    }
    dbinfo->object = std::move(previous);
    return AddResult::BACKEND_ERROR;
}

[[nodiscard]] std::optional<ChatId> ProtoDatabase::getChatId(
    const std::string_view name) const {
    std::lock_guard<std::mutex> lock(dbinfo_mutex_);
    const auto& obj = dbinfo->object;
    for (const auto& chat : obj.chattonames()) {
        if (absl::EqualsIgnoreCase(chat.name(), name)) {
            return chat.telegramchatid();
        }
    }
    return std::nullopt;
}
