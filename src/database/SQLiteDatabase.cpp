#include "SQLiteDatabase.hpp"

#include <absl/log/check.h>
#include <absl/log/log.h>

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <libos/libfs.hpp>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

#include "StringToolsExt.hpp"
#include "Types.h"

void SQLiteDatabase::Helper::logInvalidState(
    const char* func, SQLiteDatabase::Helper::State state) {
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
    }
    LOG(ERROR) << "Invalid state for " << func << ": " << stateString;
}

SQLiteDatabase::Helper::Helper(sqlite3* db, const std::string_view& filename)
    : db(db) {
    static auto sqlResPath = FS::getPathForType(FS::PathType::RESOURCES_SQL);

    std::ifstream sqlFile(sqlResPath / filename.data());
    int ret = 0;

    if (!sqlFile.is_open()) {
        LOG(ERROR) << "Could not open SQL script file: " << filename;
        throw std::runtime_error("Could not open SQL script file: " +
                                 std::string(filename));
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
        case State::PREPARED:
            LOG(WARNING) << "Statement already prepared";
            [[fallthrough]];
        case State::EXECUTED_AS_SCRIPT:
        case State::BOUND:
        case State::EXECUTED:
        case State::FAILED_TO_PREPARE:
            logInvalidState(__func__, state);
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
        case State::PREPARED:
        case State::EXECUTED_AS_SCRIPT:
        case State::BOUND:
        case State::EXECUTED:
        case State::FAILED_TO_PREPARE:
            logInvalidState(__func__, state);
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
            if (stmt != nullptr) {
                sqlite3_finalize(stmt);
            }
            break;
        case State::EXECUTED_AS_SCRIPT:
        case State::FAILED_TO_PREPARE:
            break;
    }
}

std::shared_ptr<SQLiteDatabase::Helper> SQLiteDatabase::Helper::addArgument(
    SQLiteDatabase::Helper::ArgTypes value) {
    switch (state) {
        case State::PREPARED:
            // Pass
            arguments.emplace_back(value, arguments.size() + 1);
            break;
        case State::NOTHING:
        case State::EXECUTED_AS_SCRIPT:
        case State::BOUND:
        case State::EXECUTED:
        case State::FAILED_TO_PREPARE:
            logInvalidState(__func__, state);
            break;
    };
    return shared_from_this();
}

bool SQLiteDatabase::Helper::bindArguments() {
    if (arguments.empty()) {
        LOG(ERROR) << "No arguments provided for SQL statement";
        return false;
    }

    switch (state) {
        case State::PREPARED:
            // Pass
            break;
        case State::NOTHING:
        case State::EXECUTED_AS_SCRIPT:
        case State::BOUND:
        case State::EXECUTED:
        case State::FAILED_TO_PREPARE:
            logInvalidState(__func__, state);
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
    return true;
}

std::optional<SQLiteDatabase::Helper::Row>
SQLiteDatabase::Helper::execAndGetRow() {
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
    switch (sqlite3_step(stmt)) {
        case SQLITE_DONE:
        case SQLITE_ROW:
            return true;
        default:
            LOG(ERROR) << "Error executing: " << sqlite3_errmsg(db);
            return false;
    }
    return true;
}

std::shared_ptr<SQLiteDatabase::Helper>
SQLiteDatabase::Helper::getNextStatement() {
    if (isEmptyOrBlank(scriptContentUnparsed)) {
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
    auto helper = Helper::create(db, Helper::kInsertUserFile);
    if (!helper->prepare()) {
        return ListResult::BACKEND_ERROR;
    }
    helper->addArgument(user)
        ->addArgument(static_cast<int>(type))
        ->bindArguments();
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

    auto helper = Helper::create(db, Helper::kRemoveUserFile.data());
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

void SQLiteDatabase::initDatabase() {
    auto helper = Helper::create(db, Helper::kCreateDatabaseFile.data());
    if (!helper->executeAsScript()) {
        throw std::runtime_error("Error initializing database");
    }
}

[[nodiscard]] DatabaseBase::ListResult SQLiteDatabase::checkUserInList(
    ListType type, UserId user) const {
    return checkUserInList(toInfoType(type), user);
}

[[nodiscard]] DatabaseBase::ListResult SQLiteDatabase::checkUserInList(
    InfoType type, UserId user) const {
    ListResult result = ListResult::BACKEND_ERROR;
    std::optional<Helper::Row> row;

    auto helper = Helper::create(db, Helper::kFindUserFile.data());
    if (!helper->prepare()) {
        return result;
    }
    helper->addArgument(user);
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

bool SQLiteDatabase::loadDatabaseFromFile(std::filesystem::path filepath) {
    int ret = 0;

    if (db != nullptr) {
        return false;
    }
    ret = sqlite3_open(filepath.string().c_str(), &db);
    if (ret != SQLITE_OK) {
        LOG(ERROR) << "Could not open database: " << sqlite3_errmsg(db);
        return false;
    }
    LOG(INFO) << "Loaded SQLite database: " << filepath;
    return true;
}

bool SQLiteDatabase::unloadDatabase() {
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

    auto helper = Helper::create(db, Helper::kFindMediaInfoFile);
    if (!helper->prepare()) {
        return std::nullopt;
    }
    helper->addArgument(str)->bindArguments();
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

bool SQLiteDatabase::addMediaInfo(const MediaInfo& info) const {
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

    if (info.names.size() == 0) {
        LOG(ERROR) << "Zero-length names specified";
        return false;  // No names to insert, so no need to run the query
    }

    // Determine stuff to insert, and the ones that already exist
    for (const auto& name : info.names) {
        auto helper = Helper::create(db, Helper::kFindMediaNameFile);
        if (!helper->prepare()) {
            return false;
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
                auto insertHelper =
                    Helper::create(db, Helper::kInsertMediaNameFile);
                if (!insertHelper->prepare()) {
                    return false;
                }
                insertHelper->addArgument(name);
                insertHelper->bindArguments();
                if (!insertHelper->execute()) {
                    return false;
                }

                // Get the index again
                auto findHelper =
                    Helper::create(db, Helper::kFindMediaNameFile);
                if (!findHelper->prepare()) {
                    return false;
                }
                findHelper->addArgument(name);
                findHelper->bindArguments();
                const auto row = findHelper->execAndGetRow();
                if (!row) {
                    return false;
                }
                info.data = row->get<int>(0);
                break;
            }

            case UpdateInfo::Op::USE_EXISTING:
                break;
        }
    }

    // Insert the media info into the database
    auto insertMediaHelper = Helper::create(db, Helper::kInsertMediaIdFile);

    if (!insertMediaHelper->prepare()) {
        return false;
    }
    insertMediaHelper->addArgument(info.mediaUniqueId)
        ->addArgument(info.mediaId)
        ->bindArguments();
    if (!insertMediaHelper->execute()) {
        return false;
    }

    // Get the inserted media index
    auto findMediaIdHelper = Helper::create(db, Helper::kFindMediaIdFile);
    if (!findMediaIdHelper->prepare()) {
        return false;
    }
    findMediaIdHelper->addArgument(info.mediaUniqueId)->bindArguments();
    const auto row = findMediaIdHelper->execAndGetRow();
    if (!row) {
        return false;
    }
    mediaIdIndex = row->get<int>(0);

    // Insert the actual map into the database
    for (const auto& info : updates) {
        int data = std::get<int>(info.data);

        auto insertMediaMapHelper =
            Helper::create(db, Helper::kInsertMediaMapFile);
        if (!insertMediaMapHelper->prepare()) {
            return false;
        }
        insertMediaMapHelper->addArgument(mediaIdIndex);
        insertMediaMapHelper->addArgument(data);
        insertMediaMapHelper->bindArguments();

        if (!insertMediaMapHelper->execute()) {
            return false;
        }
    }
    return true;
}

UserId SQLiteDatabase::getOwnerUserId() const {
    auto helper = Helper::create(db, Helper::kFindOwnerFile);
    if (!helper->prepare()) {
        return kInvalidUserId;
    }
    const auto row = helper->execAndGetRow();
    if (!row) {
        return kInvalidUserId;
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
    ss << "Owner Id: " << std::quoted(std::to_string(getOwnerUserId()))
       << std::endl;

    auto helper = Helper::create(db, Helper::kDumpDatabaseFile);
    if (helper->prepare()) {
        std::optional<Helper::Row> row;
        while ((row = helper->execAndGetRow())) {
            ss << "UserId: " << row->get<UserId>(0);
            int type = row->get<int>(1);
            if (type < static_cast<int>(ListType::WHITELIST) ||
                type > static_cast<int>(ListType::BLACKLIST)) {
                ss << " type: Invalid(" << type << ")" << std::endl;
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
            };
            ss << std::endl;
        }
    } else {
        ss << "!!! Failed to dump usermap database" << std::endl;
    }
    ss << std::endl;

    // Dump media database
    if (auto mhelper = helper->getNextStatement(); mhelper) {
        std::optional<Helper::Row> row;
        bool any = false;
        while ((row = mhelper->execAndGetRow())) {
            ss << "MediaId: " << row->get<std::string>(0) << std::endl;
            ss << "MediaUniqueId: " << row->get<std::string>(1) << std::endl;
            ss << "MediaName: " << row->get<std::string>(2) << std::endl;
            ss << std::endl;
            any = true;
        }
        if (!any) {
            ss << "!!! No media entries in the database" << std::endl;
        }
    } else {
        ss << "!!! Failed to dump media database" << std::endl;
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