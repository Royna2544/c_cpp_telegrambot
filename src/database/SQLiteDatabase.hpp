#pragma once

#include <TgBotDBImplExports.h>
#include <sqlite3.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "DatabaseBase.hpp"

namespace detail {

template <typename T>
inline T get(const int index, sqlite3_stmt *stmt) = delete;

template <>
[[nodiscard]] inline int32_t get(const int index, sqlite3_stmt *stmt) {
    return sqlite3_column_int(stmt, index);
}

template <>
[[nodiscard]] inline int64_t get(const int index, sqlite3_stmt *stmt) {
    return sqlite3_column_int64(stmt, index);
}

template <>
[[nodiscard]] inline std::string get(const int index, sqlite3_stmt *stmt) {
    const auto *buffer = sqlite3_column_text(stmt, index);
    if (buffer == nullptr) {
        return {};
    }
    return reinterpret_cast<const char *>(buffer);
}

}  // namespace detail

struct TgBotDBImpl_API SQLiteDatabase : DatabaseBase {
    enum class InfoType {
        MIN = -1,
        OWNER = 0,
        BLACKLIST = 1,
        WHITELIST = 2,
        MAX = 3
    };
    [[nodiscard]] ListResult addUserToList(ListType type,
                                           UserId user) const override;
    [[nodiscard]] ListResult removeUserFromList(ListType type,
                                                UserId user) const override;
    [[nodiscard]] ListResult checkUserInList(ListType type,
                                             UserId user) const override;
    bool load(std::filesystem::path filepath) override;
    bool unload() override;
    [[nodiscard]] std::optional<UserId> getOwnerUserId() const override;
    [[nodiscard]] std::optional<MediaInfo> queryMediaInfo(
        std::string str) const override;
    [[nodiscard]] AddResult addMediaInfo(const MediaInfo &info) const override;
    [[nodiscard]] std::vector<MediaInfo> getAllMediaInfos() const override;
    void setOwnerUserId(UserId userId) const override;
    std::ostream &dump(std::ostream &ofs) const override;
    [[nodiscard]] AddResult addChatInfo(
        const ChatId chatid, const std::string_view name) const override;
    [[nodiscard]] std::optional<ChatId> getChatId(
        const std::string_view name) const override;

    /**
     * SQLiteDatabase::Helper is a helper class for executing SQL statements
     * with parameters. It is designed to simplify the process of preparing and
     * executing SQL statements.
     */
    class Helper : public std::enable_shared_from_this<Helper> {
       public:
        using ArgTypes = std::variant<int32_t, int64_t, std::string_view>;

        static constexpr std::string_view kInsertUserFile = "insertUser.sql";
        static constexpr std::string_view kRemoveUserFile = "removeUser.sql";
        static constexpr std::string_view kFindUserFile = "findUser.sql";
        static constexpr std::string_view kFindMediaInfoFile =
            "findMediaInfo.sql";
        static constexpr std::string_view kInsertMediaInfoFile =
            "insertMediaInfo.sql";
        static constexpr std::string_view kCreateDatabaseFile =
            "createDatabase.sql";
        static constexpr std::string_view kFindMediaNameFile =
            "findMediaName.sql";
        static constexpr std::string_view kInsertMediaNameFile =
            "insertMediaName.sql";
        static constexpr std::string_view kInsertMediaIdFile =
            "insertMediaId.sql";
        static constexpr std::string_view kFindMediaIdFile = "findMediaId.sql";
        static constexpr std::string_view kInsertMediaMapFile =
            "insertMediaMap.sql";
        static constexpr std::string_view kFindOwnerFile = "findOwner.sql";
        static constexpr std::string_view kDumpDatabaseFile =
            "dumpDatabase.sql";
        static constexpr std::string_view kInsertChatFile = "insertChat.sql";
        static constexpr std::string_view kFindChatIdFile = "findChatId.sql";
        static constexpr std::string_view kFindAllMediaMapFile =
            "findAllMediaMap.sql";

        struct Row {
            template <typename T>
            T get(const int index) const {
                return detail::get<T>(index, stmt);
            }

            explicit Row(std::shared_ptr<Helper> inst, sqlite3_stmt *stmt)
                : _inst(std::move(inst)), stmt(stmt) {}

           private:
            // Unused, but helps to make Helper class refcnt > 0
            // As if Helper class went out of scope, stmt is free'd
            // which is not what we want as this class would be undefined
            std::shared_ptr<Helper> _inst;
            sqlite3_stmt *stmt;
        };

        ~Helper();

        /**
         * Prepares the SQL statement with the loaded SQL buffer.
         *
         * @return True if the SQL statement was successfully prepared, false
         * otherwise.
         */
        bool prepare();

        /**
         * @brief Executes the SQL script file.
         *
         * This method executes the SQL script file that was loaded earlier
         * using the constructor method. It will execute all the SQL
         * statements in the script file.
         *
         * @return True if the SQL script file was successfully executed, false
         * otherwise.
         */
        bool executeAsScript();

        /**
         * Adds an argument to the SQL statement.
         *
         * @param value The argument value, which can be of type int32_t,
         * int64_t, or std::string.
         * @return A reference to the current Helper object.
         */
        std::shared_ptr<Helper> addArgument(ArgTypes value);

        /**
         * @brief Binds the arguments to the SQL statement.
         *
         * This method binds the arguments to the SQL statement that was
         * prepared earlier using the addArgument() method.
         *
         * @return True if the arguments were successfully bound, false
         * otherwise.
         */
        bool bindArguments();

        /**
         * Executes the SQL statement that was prepared earlier using the
         * addArgument() and bindArguments() methods.
         * It doesn't care about the return value is SQLITE_ROW or SQLITE_OK.
         * Will just return false if there was an error
         */
        bool execute();

        /**
         * @brief Retrieves a row of data from the result set of the SQL
         * statement.
         *
         * This method retrieves a single row of data from the result set of the
         * SQL statement that was prepared earlier using the addArgument() and
         * bindArguments() methods. If there are no more rows to retrieve, an
         * empty optional is returned.
         * It will execute the SQL statement and return the result.
         *
         * @return A std::optional containing a Row object if a row was
         * retrieved, otherwise an empty optional.
         */
        std::optional<Row> execAndGetRow();

        /**
         * @brief Retrieves the next SQL statement from the script content.
         *
         * This method retrieves the next SQL statement from the script content
         * that was loaded earlier using the loadAndExecuteSql() method. If
         * there are no more statements to retrieve, an empty optional is
         * returned.
         *
         * @return A std::shared_ptr containing a Helper object if a statement
         * was retrieved, otherwise an empty pointer.
         */
        std::shared_ptr<Helper> getNextStatement();

        static std::shared_ptr<Helper> create(
            sqlite3 *db, const std::string_view &filename) {
            return std::make_shared<Helper>(Helper(db, filename));
        }

       private:
        /**
         * Constructor for SQLiteDatabase::Helper.
         * Private to make it only available as a shared pointer
         *
         * @param db A pointer to the SQLite database.
         * @param filename The name of the SQL script file to be executed.
         * @throws std::runtime_error if the SQL script file cannot be loaded..
         */
        explicit Helper(sqlite3 *db, const std::string_view &filename);

        // Used for methods that have multiple statements
        explicit Helper(sqlite3 *db, std::string content);

        /**
         * Structure to hold an argument for the SQL statement.
         */
        struct Argument {
            ArgTypes parameter;
            std::vector<ArgTypes>::size_type index;

            explicit Argument(ArgTypes param,
                              std::vector<ArgTypes>::size_type index)
                : parameter(std::move(param)), index(index) {}
        };

        /**
         * @brief Enum class for the different states of the
         * SQLiteDatabase::Helper.
         *
         * This enum class represents the different states that the
         * SQLiteDatabase::Helper can be in during its lifetime.
         */
        enum class State {
            NOTHING,             // Only called ctor
            PREPARED,            // Called prepare() method
            EXECUTED_AS_SCRIPT,  // Called executeAsScript() method
            HAS_ARGUMENTS,       // Called addArguments() method at least once
            BOUND,               // Called bindArguments() method
            EXECUTED,            // Called execute() or execAndGetRow() method
            FAILED_TO_PREPARE,   // Failed to prepare
        } state = State::NOTHING;

        /**
         * @brief Logs an error message for an invalid state of the
         * SQLiteDatabase::Helper.
         *
         * This method logs an error message for an invalid state of the
         * SQLiteDatabase::Helper. It is used to provide debugging information
         * when the state of the Helper object is not as expected.
         *
         * @param location The source location where the invalid state was
         * detected.
         * @param state The invalid state of the SQLiteDatabase::Helper.
         */
        static void logInvalidState(const std::source_location &location,
                                    State state);

        /**
         * @brief Checks if the SQL statement execution was successful.
         *
         * This method checks if the SQL statement execution was successful. It
         * is used to provide debugging information when the SQL statement
         * execution fails.
         *
         * @param location The source location where the SQL statement execution
         * was attempted.
         * @return True if the SQL statement could continue execution, false
         * otherwise
         */
        bool commonExecCheck(const std::source_location &location);

        std::string scriptContent;
        std::string scriptContentUnparsed;

        // Vector to store the arguments.
        std::vector<Argument> arguments;
        // Pointer to the SQLite database.
        sqlite3 *db;
        sqlite3_stmt *stmt = nullptr;
    };

   private:
    [[nodiscard]] ListResult addUserToList(InfoType type, UserId user) const;
    [[nodiscard]] ListResult checkUserInList(InfoType type, UserId user) const;
    static InfoType toInfoType(ListType type);
    sqlite3 *db = nullptr;
};