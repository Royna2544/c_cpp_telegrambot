#pragma once

#include <DBImplExports.h>

#include <filesystem>
#include <optional>
#include <ostream>
#include <string_view>
#include <vector>

#include "api/typedefs.h"

struct DBImpl_API DatabaseBase {
    virtual ~DatabaseBase() = default;
    
    struct DBImpl_API exception : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    static constexpr std::string_view kInMemoryDatabase = ":memory:";

    enum class ListType { WHITELIST, BLACKLIST };
    enum class ListResult {
        OK,
        NOT_IN_LIST,
        ALREADY_IN_LIST,
        ALREADY_IN_OTHER_LIST,
        BACKEND_ERROR
    };

    enum class MediaType {
        UNKNOWN,
        PHOTO,
        VIDEO,
        AUDIO,
        // DOCUMENT,
        STICKER = 5,
        GIF
    };

    struct MediaInfo {
        std::string mediaId;
        std::string mediaUniqueId;
        std::vector<std::string> names;
        MediaType mediaType;
    };

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
    virtual bool load(std::filesystem::path filepath) = 0;

    /**
     * @brief Unload the database from memory.
     *
     * This function should unload the database from memory and free up any
     * resources associated with it. After calling this function, the database
     * should no longer be accessible.
     */
    virtual bool unload() = 0;

    /**
     * @brief Get the user id of the owner of the database
     *
     * @return the user id of the owner of the database
     */
    [[nodiscard]] virtual std::optional<UserId> getOwnerUserId() const = 0;

    /**
     * @brief Set the user id of the owner of the database
     *
     * @param userId the user id of the owner of the database
     *
     * This may be called once. Subsequent calls will be rejected
     */
    virtual void setOwnerUserId(UserId userId) const = 0;

    /**
     * @brief Query the database for media info
     *
     * @param str the name or unique id of the media to query
     * @return std::optional<MediaInfo> the media info if found, or an empty
     * optional if not found
     */
    [[nodiscard]] virtual std::optional<MediaInfo> queryMediaInfo(
        std::string str) const = 0;

    // Return value for addXXX methods.
    enum class AddResult { OK, ALREADY_EXISTS, BACKEND_ERROR };

    /**
     * @brief Add a media info to the database
     *
     * @param info the media info to add
     * @return Result of the operation
     * `AddResult::OK' if successful
     * `AddResult::ALREADY_EXISTS' if the media info already exists in the
     * database `AddResult::BACKEND_ERROR' if a backend error occurs.
     */
    [[nodiscard]] virtual AddResult addMediaInfo(
        const MediaInfo& info) const = 0;

    /**
     * @brief Get all media infos inside the database
     *
     * @return std::vector<MediaInfo> containing all the media infos in the
     * database.
     */
    [[nodiscard]] virtual std::vector<MediaInfo> getAllMediaInfos() const = 0;

    /**
     * @brief Add a chat info to the database
     *
     * This function adds a new chat info to the database. The chat info
     * consists of a chat id and a name. If the chat info already exists in the
     * database, the function should return false.
     *
     * @param chatid The unique identifier of the chat.
     * @param name The name of the chat.
     * @return Result of the operation
     * `AddResult::OK' if successful
     * `AddResult::ALREADY_EXISTS' if the chat info already exists in the
     * database `AddResult::BACKEND_ERROR' if a backend error occurs.
     *
     * @note The chat id and name should be unique within the database.
     */
    [[nodiscard]] virtual AddResult addChatInfo(
        const ChatId chatid, const std::string_view name) const = 0;

    /**
     * @brief Get the chat id associated with a given chat name
     *
     * This function retrieves the chat id associated with a given chat name
     * from the database. If the chat name does not exist in the database, the
     * function should return std::nullopt.
     *
     * @param name The name of the chat.
     * @return The chat id associated with the given chat name. If the chat name
     * does not exist, the function should return std::nullopt.
     */
    [[nodiscard]] virtual std::optional<ChatId> getChatId(
        const std::string_view name) const = 0;

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
     * @return std::string_view simple name of the list type
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

inline std::ostream& operator<<(std::ostream& os,
                                const DatabaseBase::ListType& type) {
    switch (type) {
        case DatabaseBase::ListType::WHITELIST:
            return os << "WHITELIST";
        case DatabaseBase::ListType::BLACKLIST:
            return os << "BLACKLIST";
    }
    return os;
}

inline std::ostream& operator<<(std::ostream& os,
                                const DatabaseBase::ListResult& result) {
    switch (result) {
        case DatabaseBase::ListResult::OK:
            return os << "OK";
        case DatabaseBase::ListResult::NOT_IN_LIST:
            return os << "NOT_IN_LIST";
        case DatabaseBase::ListResult::ALREADY_IN_LIST:
            return os << "ALREADY_IN_LIST";
        case DatabaseBase::ListResult::ALREADY_IN_OTHER_LIST:
            return os << "ALREADY_IN_OTHER_LIST";
        case DatabaseBase::ListResult::BACKEND_ERROR:
            return os << "BACKEND_ERROR";
    }
    return os;
}

inline std::ostream& operator<<(std::ostream& os,
                                const DatabaseBase::MediaType& type) {
    switch (type) {
        case DatabaseBase::MediaType::UNKNOWN:
            return os << "UNKNOWN";
        case DatabaseBase::MediaType::PHOTO:
            return os << "PHOTO";
        case DatabaseBase::MediaType::VIDEO:
            return os << "VIDEO";
        case DatabaseBase::MediaType::AUDIO:
            return os << "AUDIO";
        case DatabaseBase::MediaType::STICKER:
            return os << "STICKER";
        case DatabaseBase::MediaType::GIF:
            return os << "GIF";
    }
    return os;
}