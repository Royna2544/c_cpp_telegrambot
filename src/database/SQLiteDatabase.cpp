#include "SQLiteDatabase.hpp"

#include <absl/log/check.h>
#include <absl/log/log.h>
#include <absl/strings/ascii.h>
#include <fmt/core.h>

#include <StacktracePrint.hpp>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <libfs.hpp>
#include <map>
#include <memory>
#include <optional>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <variant>

#include "api/typedefs.h"

namespace {
bool backtracePrint(const std::string_view& entry) {
    LOG(ERROR) << entry;
    return entry.find("SQLiteDatabase") != std::string::npos;
}
}  // namespace

void SQLiteDatabase::Helper::logInvalidState(
    const std::source_location& location, SQLiteDatabase::Helper::State state) {
    std::string_view stateString;
    switch (state) {
        case State::NOTHING:
            stateString = "NOTHING";
            break;
        case State::PREPARED:
            stateString = "PREPARED";
            break;
        case State::EXECUTED_AS_SCRIPT:
            stateString = "EXECUTED_AS_SCRIPT";
            break;
        case State::BOUND:
            stateString = "BOUND";
            break;
        case State::EXECUTED:
            stateString = "EXECUTED";
            break;
        case State::FAILED_TO_PREPARE:
            stateString = "FAILED";
            break;
        case State::HAS_ARGUMENTS:
            stateString = "HAS_ARGUMENTS";
            break;
    }
    LOG(ERROR) << "Invalid state for " << location.function_name() << ": "
               << stateString;
    PrintStackTrace(backtracePrint);
}

SQLiteDatabase::Helper::Helper(sqlite3* db,
                               const std::filesystem::path& sqlScriptPath,
                               const std::string_view& filename)
    : db(db) {
    std::ifstream sqlFile(sqlScriptPath / filename.data());
    int ret = 0;

    if (!sqlFile.is_open()) {
        throw exception(fmt::format("Could not open SQL script file: {}", filename));
    }
    scriptContent = std::string((std::istreambuf_iterator<char>(sqlFile)),
                                std::istreambuf_iterator<char>());
}

bool SQLiteDatabase::Helper::prepare() {
    const char* pztail = nullptr;

    switch (state) {
        case State::NOTHING:
            // Pass
            break;
        default:
            logInvalidState(std::source_location::current(), state);
            return false;
    };
    auto ret =
        sqlite3_prepare_v2(db, scriptContent.c_str(), -1, &stmt, &pztail);
    if (ret != SQLITE_OK) {
        LOG(ERROR) << "Failed to prepare statement: " << sqlite3_errmsg(db);
        state = State::FAILED_TO_PREPARE;
        return false;
    }
    state = State::PREPARED;
    if (pztail != nullptr) {
        scriptContentUnparsed = pztail;
    }
    return true;
}

bool SQLiteDatabase::Helper::executeAsScript() {
    char* err_message = nullptr;

    switch (state) {
        case State::NOTHING:
            // Pass
            break;
        default:
            logInvalidState(std::source_location::current(), state);
            return false;
    };

    state = State::EXECUTED_AS_SCRIPT;
    LOG(INFO) << "Executing SQL script...";
    if (sqlite3_exec(db, scriptContent.c_str(), nullptr, nullptr,
                     &err_message) != SQLITE_OK) {
        LOG(ERROR) << "Failed to execute SQL script: " << err_message;
        sqlite3_free(err_message);
        return false;
    } else {
        DLOG(INFO) << "Executed SQL script successfully.";
    }
    return true;
}

SQLiteDatabase::Helper::Helper(sqlite3* db, std::string content)
    : db(db), scriptContent(std::move(content)) {}

SQLiteDatabase::Helper::~Helper() {
    switch (state) {
        case State::NOTHING:
            // No cleanup needed
            break;
        case State::BOUND:
        case State::EXECUTED:
        case State::PREPARED:
        case State::HAS_ARGUMENTS:
            if (stmt != nullptr) {
                sqlite3_finalize(stmt);
            }
            break;
        case State::EXECUTED_AS_SCRIPT:
        case State::FAILED_TO_PREPARE:
            break;
    }
}

SQLiteDatabase::Helper& SQLiteDatabase::Helper::addArgument(
    const ArgTypes& value) {
    switch (state) {
        case State::HAS_ARGUMENTS:
        case State::PREPARED:
            // Pass
            arguments.emplace_back(value, arguments.size() + 1);
            break;
        default:
            logInvalidState(std::source_location::current(), state);
            break;
    };
    state = State::HAS_ARGUMENTS;
    return *this;
}

bool SQLiteDatabase::Helper::bindArguments() {
    switch (state) {
        case State::HAS_ARGUMENTS:
            // Pass
            break;
        case State::PREPARED:
            LOG(WARNING) << __func__ << " called without any arguments added?";
            return false;
        default:
            logInvalidState(std::source_location::current(), state);
            return false;
    };

    for (const auto& argument : arguments) {
        std::visit(
            [this, &argument](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, std::string>) {
                    sqlite3_bind_text(stmt, argument.index, arg.c_str(), -1,
                                      SQLITE_STATIC);
                } else if constexpr (std::is_same_v<T, int64_t>) {
                    sqlite3_bind_int64(stmt, argument.index, arg);
                } else if constexpr (std::is_same_v<T, int32_t>) {
                    sqlite3_bind_int(stmt, argument.index, arg);
                }
            },
            argument.parameter);
    }
    state = State::BOUND;
    return true;
}

bool SQLiteDatabase::Helper::commonExecCheck(
    const std::source_location& location) {
    switch (state) {
        case State::BOUND:
        case State::PREPARED:
        case State::EXECUTED:
            // Pass
            return true;
        case State::HAS_ARGUMENTS:
            LOG(WARNING) << location.function_name()
                         << " called with added arguments, but is not bound?";
            PrintStackTrace(backtracePrint);
            bindArguments();
            return true;
        default:
            logInvalidState(location, state);
            return false;
    }
}

std::optional<SQLiteDatabase::Helper::Row>
SQLiteDatabase::Helper::execAndGetRow() {
    if (!commonExecCheck(std::source_location::current())) {
        return std::nullopt;
    }

    state = State::EXECUTED;
    switch (sqlite3_step(stmt)) {
        case SQLITE_ROW: {
            Row row{shared_from_this(), stmt};
            return row;
        }
        case SQLITE_DONE:
            return std::nullopt;
        default:
            LOG(ERROR) << "Error fetching row: " << sqlite3_errmsg(db);
            return std::nullopt;
    }
}

bool SQLiteDatabase::Helper::execute() {
    if (!commonExecCheck(std::source_location::current())) {
        return false;
    }

    switch (sqlite3_step(stmt)) {
        case SQLITE_DONE:
        case SQLITE_ROW:
            return true;
        default:
            LOG(ERROR) << "Error executing: " << sqlite3_errmsg(db);
            return false;
    }
    state = State::EXECUTED;
    return true;
}

std::shared_ptr<SQLiteDatabase::Helper>
SQLiteDatabase::Helper::getNextStatement() {
    switch (state) {
        case State::EXECUTED:
            break;
        default:
            logInvalidState(std::source_location::current(), state);
            return nullptr;
    }

    if (absl::StripAsciiWhitespace(scriptContentUnparsed).empty()) {
        return nullptr;
    }
    return std::make_shared<SQLiteDatabase::Helper>(
        Helper{db, scriptContentUnparsed});
}

SQLiteDatabase::ListResult SQLiteDatabase::addUserToList(InfoType type,
                                                         UserId user) const {
    ListResult res{};

    res = checkUserInList(type, user);
    switch (res) {
        case ListResult::NOT_IN_LIST:
            break;
        case ListResult::ALREADY_IN_LIST:
            LOG(ERROR) << "Unexpected list result type "
                       << static_cast<int>(type) << " for user " << user;
            return res;
        case ListResult::OK:
            res = ListResult::ALREADY_IN_LIST;
            [[fallthrough]];
        case ListResult::ALREADY_IN_OTHER_LIST:
        case ListResult::BACKEND_ERROR:
            return res;
    }
    auto helper = Helper::create(db, _sqlScriptsPath, Helper::kInsertUserFile);
    if (!helper->prepare()) {
        return ListResult::BACKEND_ERROR;
    }
    helper->addArgument(user)
        .addArgument(static_cast<int>(type))
        .bindArguments();
    if (helper->execute()) {
        return ListResult::OK;
    }
    return ListResult::BACKEND_ERROR;
}

SQLiteDatabase::ListResult SQLiteDatabase::addUserToList(ListType type,
                                                         UserId user) const {
    return addUserToList(toInfoType(type), user);
}

SQLiteDatabase::ListResult SQLiteDatabase::removeUserFromList(
    ListType type, UserId user) const {
    ListResult res{};

    res = checkUserInList(type, user);
    switch (res) {
        case ListResult::OK:
            break;
        case ListResult::ALREADY_IN_LIST:
            LOG(ERROR) << "Unexpected list result type "
                       << static_cast<int>(type) << " for user " << user;
            [[fallthrough]];
        case ListResult::NOT_IN_LIST:
        case ListResult::ALREADY_IN_OTHER_LIST:
        case ListResult::BACKEND_ERROR:
            return res;
    }

    auto helper =
        Helper::create(db, _sqlScriptsPath, Helper::kRemoveUserFile.data());
    if (!helper->prepare()) {
        return ListResult::BACKEND_ERROR;
    }
    helper->addArgument(user);
    helper->addArgument(static_cast<int>(toInfoType(type)));
    helper->bindArguments();
    if (helper->execute()) {
        return ListResult::OK;
    }
    return ListResult::BACKEND_ERROR;
}

[[nodiscard]] DatabaseBase::ListResult SQLiteDatabase::checkUserInList(
    ListType type, UserId user) const {
    return checkUserInList(toInfoType(type), user);
}

[[nodiscard]] DatabaseBase::ListResult SQLiteDatabase::checkUserInList(
    InfoType type, UserId user) const {
    ListResult result = ListResult::BACKEND_ERROR;
    std::optional<Helper::Row> row;

    auto helper =
        Helper::create(db, _sqlScriptsPath, Helper::kFindUserFile.data());
    if (!helper->prepare()) {
        return result;
    }
    helper->addArgument(user).bindArguments();
    row = helper->execAndGetRow();
    if (row) {
        const auto info = static_cast<InfoType>(row->get<int>(0));
        const auto reqinfo = type;
        if (info == reqinfo) {
            result = ListResult::OK;
        } else {
            // Not in this list, but other
            result = ListResult::ALREADY_IN_OTHER_LIST;
        }
    } else {
        // Not in this list, just doesn't exist
        result = ListResult::NOT_IN_LIST;
    }
    return result;
}

bool SQLiteDatabase::load(std::filesystem::path filepath) {
    int ret = 0;
    std::error_code ec;

    if (db != nullptr) {
        LOG(WARNING) << "Attempting to load database while it is already open.";
        return false;
    }

    bool existed = std::filesystem::exists(filepath, ec);
    if (ec) {
        LOG(ERROR) << "Failed to check if file exists: " << filepath << ": "
                   << ec.message();
        return false;
    }

    ret = sqlite3_open(filepath.string().c_str(), &db);
    if (ret != SQLITE_OK) {
        LOG(ERROR) << "Could not open database: " << sqlite3_errmsg(db);
        return false;
    }

    // Check if the database exists and initialize it if necessary.
    if (!existed || filepath == kInMemoryDatabase) {
        LOG(INFO) << "Running initialization script...";
        const auto helper = Helper::create(db, _sqlScriptsPath,
                                           Helper::kCreateDatabaseFile.data());
        if (!helper->executeAsScript()) {
            throw std::runtime_error("Error initializing database");
        }
    }
    if (filepath != kInMemoryDatabase) {
        LOG(INFO) << "Loaded SQLite database: " << filepath;
    } else {
        LOG(INFO) << "Loaded in-memory SQLite database";
    }
    return true;
}

bool SQLiteDatabase::unload() {
    if (db != nullptr) {
        if (sqlite3_close(db) == SQLITE_OK) {
            db = nullptr;
            return true;
        } else {
            LOG(ERROR) << "Could not close database: " << sqlite3_errmsg(db);
            return false;
        }
    }
    return false;
}

std::optional<SQLiteDatabase::MediaInfo> SQLiteDatabase::queryMediaInfo(
    std::string str) const {
    MediaInfo info{};

    auto helper =
        Helper::create(db, _sqlScriptsPath, Helper::kFindMediaInfoFile);
    if (!helper->prepare()) {
        return std::nullopt;
    }
    helper->addArgument(str).bindArguments();
    auto row = helper->execAndGetRow();

    if (row) {
        info.mediaId = row->get<std::string>(0);
        info.mediaUniqueId = row->get<std::string>(1);
    } else {
        LOG(ERROR) << "Didn't find media info for name: " << str;
        return std::nullopt;
    }
    return info;
}

SQLiteDatabase::AddResult SQLiteDatabase::addMediaInfo(
    const MediaInfo& info) const {
    struct UpdateInfo {
        enum class Op {
            INSERT,  // This name does not exist in namemap: I should insert it
            USE_EXISTING,  // This name exists in namemap
        } update{};
        // id of namemap for USE_EXISTING, string data for INSERT
        std::variant<int, std::string> data;
    };
    int count = 0;
    int mediaIdIndex = 0;
    std::vector<UpdateInfo> updates(info.names.size());
    MediaType type = info.mediaType;

    if (info.names.size() == 0) {
        LOG(ERROR) << "Zero-length names specified";
        // No names to insert, so no need to run the query
        return AddResult::BACKEND_ERROR;
    }

    // Determine stuff to insert, and the ones that already exist
    for (const auto& name : info.names) {
        auto helper =
            Helper::create(db, _sqlScriptsPath, Helper::kFindMediaNameFile);
        if (!helper->prepare()) {
            return AddResult::BACKEND_ERROR;
        }
        helper->addArgument(name);
        helper->bindArguments();

        auto row = helper->execAndGetRow();
        if (row) {
            updates[count].update = UpdateInfo::Op::USE_EXISTING;
            updates[count].data = row->get<int>(0);
        } else {
            updates[count].update = UpdateInfo::Op::INSERT;
            updates[count].data = name;
        }
        count++;
    }

    // Insert the names into the database
    for (auto& info : updates) {
        switch (info.update) {
            case UpdateInfo::Op::INSERT: {
                const auto name = std::get<std::string>(info.data);

                // Insert into database
                auto insertHelper = Helper::create(
                    db, _sqlScriptsPath, Helper::kInsertMediaNameFile);
                if (!insertHelper->prepare()) {
                    return AddResult::BACKEND_ERROR;
                }
                insertHelper->addArgument(name);
                insertHelper->bindArguments();
                if (!insertHelper->execute()) {
                    // We could not insert the media name here.
                    // And this cannot be unique constraint issue, as we checked
                    // it above.
                    LOG(ERROR) << "Could not insert media name: " << name;
                    return AddResult::BACKEND_ERROR;
                }

                // Get the index again
                auto findHelper = Helper::create(db, _sqlScriptsPath,
                                                 Helper::kFindMediaNameFile);
                if (!findHelper->prepare()) {
                    return AddResult::BACKEND_ERROR;
                }
                findHelper->addArgument(name);
                findHelper->bindArguments();
                const auto row = findHelper->execAndGetRow();
                if (!row) {
                    // The name was inserted, and we are just trying to get the
                    // index. This should not happen, but we check it just in
                    // case.
                    LOG(ERROR) << "Could not find inserted media name index";
                    return AddResult::BACKEND_ERROR;
                }
                info.data = row->get<int>(0);
                break;
            }

            case UpdateInfo::Op::USE_EXISTING:
                break;
        }
    }

    // Insert the media info into the database
    auto insertMediaHelper =
        Helper::create(db, _sqlScriptsPath, Helper::kInsertMediaIdFile);

    if (!insertMediaHelper->prepare()) {
        return AddResult::BACKEND_ERROR;
    }
    insertMediaHelper->addArgument(info.mediaUniqueId)
        .addArgument(info.mediaId)
        .bindArguments();
    if (!insertMediaHelper->execute()) {
        // This is most likely at a fault at unique constraint violation.
        // TODO: Undo the media names adding here.
        // In this case, we return ALREADY_EXISTS
        return AddResult::ALREADY_EXISTS;
    }

    // Get the inserted media index
    auto findMediaIdHelper =
        Helper::create(db, _sqlScriptsPath, Helper::kFindMediaIdFile);
    if (!findMediaIdHelper->prepare()) {
        return AddResult::BACKEND_ERROR;
    }
    findMediaIdHelper->addArgument(info.mediaUniqueId).bindArguments();
    const auto row = findMediaIdHelper->execAndGetRow();
    if (!row) {
        // The mediaids was inserted, and we are just trying to get the index.
        // This should not happen, but we check it just in case.
        LOG(ERROR) << "Could not find inserted media id index";
        return AddResult::BACKEND_ERROR;
    }
    mediaIdIndex = row->get<int>(0);

    // Insert the actual mediaindex-name-type into the database
    for (const auto& info : updates) {
        int data = std::get<int>(info.data);

        auto insertMediaMapHelper =
            Helper::create(db, _sqlScriptsPath, Helper::kInsertMediaMapFile);
        if (!insertMediaMapHelper->prepare()) {
            return AddResult::BACKEND_ERROR;
        }
        insertMediaMapHelper->addArgument(mediaIdIndex)
            .addArgument(data)
            .addArgument(static_cast<int>(type))
            .bindArguments();

        if (!insertMediaMapHelper->execute()) {
            LOG(ERROR) << "Failed to insert the final media map";
            return AddResult::BACKEND_ERROR;
        }
    }
    return AddResult::OK;
}

std::vector<SQLiteDatabase::MediaInfo> SQLiteDatabase::getAllMediaInfos()
    const {
    using MergeMap = std::map<std::pair<std::string, std::string>,
                              std::pair<std::vector<std::string>, MediaType>>;
    MergeMap map;
    std::vector<MediaInfo> result;

    auto helper =
        Helper::create(db, _sqlScriptsPath, Helper::kFindAllMediaMapFile);
    if (!helper->prepare()) {
        return result;
    }
    while (auto rows = helper->execAndGetRow()) {
        MediaInfo info;
        info.mediaId = rows->get<std::string>(0);
        info.mediaUniqueId = rows->get<std::string>(1);
        info.names.emplace_back(rows->get<std::string>(2));
        info.mediaType = static_cast<MediaType>(rows->get<int>(3));
        result.emplace_back(info);
    }
    for (const auto& info : result) {
        auto key = std::make_pair(info.mediaUniqueId, info.mediaId);
        map[key].first.insert(map[key].first.end(), info.names.begin(),
                              info.names.end());
        map[key].second = info.mediaType;
    }
    result.clear();
    for (const auto& pair : map) {
        MediaInfo info;
        info.mediaId = pair.first.second;
        info.mediaUniqueId = pair.first.first;
        info.names = pair.second.first;
        info.mediaType = pair.second.second;
        result.emplace_back(info);
    }
    return result;
}

std::optional<UserId> SQLiteDatabase::getOwnerUserId() const {
    auto helper = Helper::create(db, _sqlScriptsPath, Helper::kFindOwnerFile);
    if (!helper->prepare()) {
        return std::nullopt;
    }
    const auto row = helper->execAndGetRow();
    if (!row) {
        return std::nullopt;
    }
    return row->get<UserId>(0);
}

SQLiteDatabase::InfoType SQLiteDatabase::toInfoType(ListType type) {
    switch (type) {
        case ListType::BLACKLIST:
            return InfoType::BLACKLIST;
        case ListType::WHITELIST:
            return InfoType::WHITELIST;
    }
    CHECK(false) << "Unreachable";
}

std::ostream& SQLiteDatabase::dump(std::ostream& ofs) const {
    std::stringstream ss;

    if (db == nullptr) {
        ofs << "Database not loaded!";
        return ofs;
    }

    ss << "====================== Dump of database ======================"
       << std::endl;

    // Because of the race condition with logging, use stringstream and output
    // later.
    ss << fmt::format("Owner Id: {}\n", getOwnerUserId().value_or(0));

    auto helper =
        Helper::create(db, _sqlScriptsPath, Helper::kDumpDatabaseFile);
    if (helper->prepare()) {
        std::optional<Helper::Row> row;
        while ((row = helper->execAndGetRow())) {
            ss << "UserId: " << row->get<UserId>(0);
            int type = row->get<int>(1);
            if (type <= static_cast<int>(InfoType::MIN) ||
                type >= static_cast<int>(InfoType::MAX)) {
                ss << fmt::format(" type: Invalid({})\n", type);
                continue;
            }
            auto info = static_cast<InfoType>(type);
            ss << " type: ";
            switch (info) {
                case InfoType::BLACKLIST:
                    ss << "BLACKLIST";
                    break;
                case InfoType::WHITELIST:
                    ss << "WHITELIST";
                    break;
                case InfoType::OWNER:
                    ss << "OWNER";
                    break;
                case InfoType::MIN:
                case InfoType::MAX:
                    break;
            };
            ss << std::endl;
        }
    } else {
        ss << "!!! Failed to dump usermap database" << std::endl;
    }
    ss << std::endl;

    // Dump media database
    if (helper = helper->getNextStatement(); helper && helper->prepare()) {
        std::optional<Helper::Row> row;
        bool any = false;
        while ((row = helper->execAndGetRow())) {
            ss << "MediaId: " << row->get<std::string>(0) << std::endl;
            ss << "MediaUniqueId: " << row->get<std::string>(1) << std::endl;
            ss << "MediaName: " << row->get<std::string>(2) << std::endl;
            ss << "MediaType: " << static_cast<MediaType>(row->get<int>(3))
               << std::endl;
            ss << std::endl;
            any = true;
        }
        if (!any) {
            ss << "!!! No media entries in the database" << std::endl;
        }
    } else {
        ss << "!!! Failed to dump media database" << std::endl;
    }
    ss << std::endl;

    // Dump chatid database
    if (helper = helper->getNextStatement(); helper && helper->prepare()) {
        std::optional<Helper::Row> row;
        bool any = false;
        while ((row = helper->execAndGetRow())) {
            ss << "ChatId: " << row->get<ChatId>(0) << std::endl;
            ss << "ChatName: " << row->get<std::string>(1) << std::endl;
            ss << std::endl;
            any = true;
        }
        if (!any) {
            ss << "!!! No chatid entries in the database" << std::endl;
        }
    } else {
        ss << "!!! Failed to dump chatid database" << std::endl;
    }

    ss << "========================= End of dump ========================"
       << std::endl;
    ofs << ss.str();
    return ofs;
}

void SQLiteDatabase::setOwnerUserId(UserId userId) const {
    switch (addUserToList(InfoType::OWNER, userId)) {
        case DatabaseBase::ListResult::OK:
            LOG(INFO) << "Owner set to " << userId;
            break;
        case DatabaseBase::ListResult::ALREADY_IN_OTHER_LIST:
        case DatabaseBase::ListResult::BACKEND_ERROR:
            LOG(ERROR) << "Failed to set owner to " << userId;
            break;
        case DatabaseBase::ListResult::ALREADY_IN_LIST:
            DLOG(INFO) << "Owner already set to " << userId;
            break;
        case DatabaseBase::ListResult::NOT_IN_LIST:
            // Not possible
            break;
    }
}

SQLiteDatabase::AddResult SQLiteDatabase::addChatInfo(
    const ChatId chatid, const std::string_view name) const {
    if (getChatId(name)) {
        return AddResult::ALREADY_EXISTS;
    }
    auto insertHelper =
        Helper::create(db, _sqlScriptsPath, Helper::kInsertChatFile);
    if (!insertHelper->prepare()) {
        return AddResult::BACKEND_ERROR;
    }
    insertHelper->addArgument(chatid)
        .addArgument(std::string(name))
        .bindArguments();
    if (insertHelper->execute()) {
        return AddResult::OK;
    } else {
        return AddResult::BACKEND_ERROR;
    }
}

std::optional<ChatId> SQLiteDatabase::getChatId(
    const std::string_view name) const {
    auto helper = Helper::create(db, _sqlScriptsPath, Helper::kFindChatIdFile);
    if (!helper->prepare()) {
        return std::nullopt;
    }
    helper->addArgument(std::string(name));
    helper->bindArguments();

    auto row = helper->execAndGetRow();
    if (!row) {
        return std::nullopt;
    }
    return row->get<ChatId>(0);
}

SQLiteDatabase::SQLiteDatabase(std::filesystem::path sqlScriptDirectory)
    : _sqlScriptsPath(std::move(sqlScriptDirectory)) {
    DLOG(INFO) << "Init with sql script directory " << _sqlScriptsPath;
}