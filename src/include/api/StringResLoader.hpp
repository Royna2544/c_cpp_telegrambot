#pragma once

#include <exception>
#include <filesystem>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <trivial_helpers/fruit_inject.hpp>
#include <unordered_map>

// clang-format off
#define MAKE_STRINGS(x)                                                        \
    x(__INVALID__)                                                             \
    x(WORKING_ON_IT)                                                           \
    x(NEED_A_NONEMPTY_REPLY_MESSAGE)                                           \
    x(COMMAND_IS)                                                              \
    x(USER_REMOVED)                                                            \
    x(USER_NOT_IN_LIST)                                                        \
    x(ANIMATED_STICKERS_NOT_SUPPORTED)                                         \
    x(USER_ALREADY_IN_OTHER_LIST)                                              \
    x(USER_ALREADY_IN_LIST)                                                    \
    x(USER_ADDED)                                                              \
    x(UNKNOWN_ERROR)                                                           \
    x(CREATING_NEW_STICKER_SET)                                                \
    x(FAILED_TO_CREATE_NEW_STICKER_SET)                                        \
    x(UNKNOWN_ACTION)                                                          \
    x(UNKNOWN)                                                                 \
    x(SEND_POSSIBILITIES)                                                      \
    x(SEND_BASH_COMMAND)                                                       \
    x(SEND_A_NAME_TO_SAVE)                                                     \
    x(RUN_TIME)                                                                \
    x(REPLY_TO_USER_MSG)                                                       \
    x(REPLY_TO_A_MEDIA)                                                        \
    x(REPLY_TO_A_STICKER)                                                      \
    x(REPLY_TO_GIF_OR_STICKER)                                                 \
    x(STARTING_CONVERSION)                                                     \
    x(FAILED_TO_CREATE_DIRECTORY)                                              \
    x(DECIDING)                                                                \
    x(YES)                                                                     \
    x(NO)                                                                      \
    x(SO_YES)                                                                  \
    x(SO_NO)                                                                   \
    x(SO_IDK)                                                                  \
    x(SHORT_CIRCUITED_TO_THE_ANSWER)                                           \
    x(FAILED_TO_DOWNLOAD_FILE)                                                 \
    x(FAILED_TO_READ_FILE)                                                     \
    x(FAILED_TO_UPLOAD_FILE)                                                   \
    x(DONE_TOOK)                                                               \
    x(ERROR_TOOK)                                                              \
    x(OUTPUT_IS_EMPTY)                                                         \
    x(REPLY_TO_A_CODE)                                                         \
    x(OPERATION_SUCCESSFUL)                                                    \
    x(OPERATION_FAILURE)                                                       \
    x(CONVERSION_IN_PROGRESS)                                                  \
    x(MEDIA_ALREADY_IN_DB)                                                     \
    x(INVALID_ARGS_PASSED)                                                     \
    x(STICKER_SET_NOT_FOUND)                                                   \
    x(FAILED_TO_WRITE_FILE)                                                    \
    x(FAILED_TO_PARSE_INPUT)                                                   \
    x(EXAMPLE)                                                                 \
    x(ERROR_SENDING_GIF)                                                       \
    x(COMPILE_TIME)                                                            \
    x(GIVE_MORE_THAN_ONE)                                                      \
    x(TOTAL_ITEMS_PREFIX)                                                      \
    x(TOTAL_ITEMS_SUFFIX)                                                      \
    x(BOT_OWNER_SET)                                                           \
    x(SEND_A_FILE_NAME_TO_FLASH)                                               \
    x(INVALID_INPUT_NO_NEWLINE)                                                \
    x(FAILED_SUCCESSFULLY)                                                     \
    x(REASON)                                                                  \
    x(SUCCESS_CHANCE_WAS)                                                      \
    x(RESTARTING_BOT)                                                          \
    x(CANNOT_ROTATE_NONSTATIC)                                                 \
    x(INVALID_ANGLE)                                                           \
    x(ROTATED_PICTURE)                                                         \
    x(FAILED_TO_ROTATE_IMAGE)                                                  \
    x(BACKEND_ERROR)                                                           \
    x(STICKER_PACK_CREATED)                                                    \
    x(FLASHING_ZIP)                                                            \
    x(PROCESS_EXITED)                                                          \
    x(__MAX__)
// clang-format on

enum class Strings {
#define X(name) name,
    MAKE_STRINGS(X)
};
#undef X

inline std::ostream& operator<<(std::ostream& os, const Strings string) {
    switch (string) {
#define fn(x)        \
    case Strings::x: \
        os << #x;    \
        break;
        MAKE_STRINGS(fn);
#undef fn
        default:
            os << "STRINGRES_UNKNOWN";
    }
    return os;
}

class StringResLoader {
   public:
    class PerLocaleMap {
       public:
        virtual std::string_view get(const Strings key) const = 0;
    };
    class PerLocaleMapImpl : public PerLocaleMap {
        std::unordered_map<Strings, std::string> m_map;

       public:
        std::string_view get(const Strings key) const override {
            return m_map.at(key);
        }
        void reserve(const decltype(m_map)::size_type size) {
            m_map.reserve(size);
        }
        template <typename... Args>
        auto emplace(Args&&... args) {
            return m_map.emplace(args...);
        }
        bool contains(Strings key) { return m_map.contains(key); }
        explicit PerLocaleMapImpl(decltype(m_map) map)
            : m_map(std::move(map)) {}
        PerLocaleMapImpl() = default;
    };

   private:
    std::unordered_map<std::string, PerLocaleMapImpl> localeMap;
    std::optional<std::string> default_locale;
    std::filesystem::path m_path;

   public:
    // Load XML files from the specified path.
    explicit StringResLoader(std::filesystem::path path);
    ~StringResLoader();

    const PerLocaleMap* at(const std::string& key) const;
};
