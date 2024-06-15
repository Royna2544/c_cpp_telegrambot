#pragma once

#include <filesystem>
#include <optional>
#include <ostream>
#include <string_view>

#include "Types.h"

struct DatabaseBase {
    DatabaseBase() = default;
    virtual ~DatabaseBase() = default;

    enum class ListType { WHITELIST, BLACKLIST };
    enum class ListResult {
        OK,
        NOT_IN_LIST,
        ALREADY_IN_LIST,
        ALREADY_IN_OTHER_LIST,
        BACKEND_ERROR
    };

    struct MediaInfo {
        std::string mediaId;
        std::string mediaUniqueId;
        std::string names;
    };

    /**
     * @brief A constant representing an invalid user id.
     */
    static constexpr UserId kInvalidUserId = -1;

    /**
     * @brief Initialize the database.
     *
     * This function should initialize the database, creating any necessary
     * structures. This function should be called before any other functions
     * in the database are used.
     */
    virtual void initDatabase() {}

    /**
     * @brief Add a user to the database list
     *
     * @param type type of the list
     * @param user user to add
     * @return ListResult result of the operation.
     * Possible values are OK, ALREADY_IN_LIST, ALREADY_IN_OTHER_LIST,
     * BACKEND_ERROR
     */
    [[nodiscard]] virtual ListResult addUserToList(ListType type,
                                                   UserId user) const = 0;

    /**
     * @brief Remove a user from the database list
     *
     * @param type type of the list
     * @param user user to remove
     * @return ListResult result of the operation.
     * Possible values are OK, NOT_IN_LIST, ALREADY_IN_OTHER_LIST, BACKEND_ERROR
     */
    [[nodiscard]] virtual ListResult removeUserFromList(ListType type,
                                                        UserId user) const = 0;

    /**
     * @brief Check if a user is in a list
     *
     * @param type type of the list
     * @param user user to check
     * @return ListResult result of the operation.
     * Possible values are OK, NOT_IN_LIST, ALREADY_IN_OTHER_LIST, BACKEND_ERROR
     */
    [[nodiscard]] virtual ListResult checkUserInList(ListType type,
                                                     UserId user) const = 0;

    /**
     * @brief Load the database from a file.
     *
     * This function should load the database from a file on the disk. The file
     * path should be provided as a parameter to the function. If the file does
     * not exist, the function should return without loading the database.
     *
     * @param filepath Path to the file containing the database.
     */
    virtual bool loadDatabaseFromFile(std::filesystem::path filepath) = 0;

    /**
     * @brief Unload the database from memory.
     *
     * This function should unload the database from memory and free up any
     * resources associated with it. After calling this function, the database
     * should no longer be accessible.
     */
    virtual bool unloadDatabase() = 0;

    /**
     * @brief Get the user id of the owner of the database
     *
     * @return the user id of the owner of the database
     */
    [[nodiscard]] virtual UserId getOwnerUserId() const = 0;

    /**
     * @brief Query the database for media info
     *
     * @param str the name or unique id of the media to query
     * @return std::optional<MediaInfo> the media info if found, or an empty
     * optional if not found
     */
    [[nodiscard]] virtual std::optional<MediaInfo> queryMediaInfo(
        std::string str) const = 0;

    /**
     * @brief Add a media info to the database
     *
     * @param info the media info to add
     * @return true if the media info was added, false if it already exists
     */
    [[nodiscard]] virtual bool addMediaInfo(const MediaInfo& info) const = 0;

    /**
     * @brief Dump the database to the specified output stream.
     *
     * This function should dump the contents of the database to the specified
     * output stream. The output stream should be provided as a parameter to the
     * function. The dumped data should be in a format that can be easily parsed
     * and understood by humans.
     *
     * @param out The output stream to which the database will be dumped.
     * @return A reference to the output stream for method chaining.
     */
    virtual std::ostream& dump(std::ostream& out) const = 0;
    
    /**
     * @brief Get the simple name of a list type
     *
     * @param type type of the list
     * @return const char* simple name of the list type
     */
    [[nodiscard]] static std::string_view getSimpleName(ListType type) {
        switch (type) {
            case ListType::WHITELIST:
                return "whitelist";
            case ListType::BLACKLIST:
                return "blacklist";
        }
    }
};