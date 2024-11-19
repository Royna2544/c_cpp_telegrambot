#pragma once

#include <ostream>
#include <string_view>

enum class Locale {
    Default,
    en_US = Default,
    fr_FR,
    ko_KR
    // Add more locales as needed
};

inline constexpr bool operator==(const Locale& lhs,
                                 const std::string_view& rhs) {
    switch (lhs) {
        case Locale::en_US:
            return rhs == "en-US";
        case Locale::fr_FR:
            return rhs == "fr-FR";
        case Locale::ko_KR:
            return rhs == "ko-KR";
        default:
            // Add more cases as needed
            return false;
    }
}

namespace locale {

// This is used for assigning, not comparing.
inline Locale fromString(const std::string_view locale) {
    Locale lhs;
    if (locale == "en") {
        lhs = Locale::en_US;
    } else if (locale == "fr") {
        lhs = Locale::fr_FR;
    } else if (locale == "ko") {
        lhs = Locale::ko_KR;
    } else {
        lhs = Locale::Default;
    }
    return lhs;
}

}  // namespace locale

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
