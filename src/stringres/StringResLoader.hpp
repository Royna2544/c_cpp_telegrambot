#pragma once

#include <exception>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <trivial_helpers/fruit_inject.hpp>

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

// This is used for assigning, not comparing.
inline constexpr void operator<=(Locale& lhs, const std::string_view& rhs) {
    if (rhs == "en") {
        lhs = Locale::en_US;
    } else if (rhs == "fr") {
        lhs = Locale::fr_FR;
    } else if (rhs == "ko") {
        lhs = Locale::ko_KR;
    } else {
        lhs = Locale::Default;
    }
}

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
    x(__MAX__)
// clang-format on

enum class Strings {
#define X(name) name,
    MAKE_STRINGS(X)
};
#undef X

struct StringResLoaderBase {
    virtual ~StringResLoaderBase() = default;

    struct LocaleStrings {
        virtual ~LocaleStrings() = default;
        virtual std::string_view get(const Strings& string) const = 0;
        virtual size_t size() const noexcept = 0;
    };

    virtual const LocaleStrings* at(const Locale key) const = 0;
    virtual size_t size() const noexcept = 0;
};

class LocaleStringsImpl : public StringResLoaderBase::LocaleStrings {
   public:
    explicit LocaleStringsImpl(const std::filesystem::path& filepath);
    LocaleStringsImpl() = default;
    ~LocaleStringsImpl() override = default;

    class invalid_xml_error : public std::exception {
       public:
        explicit invalid_xml_error() : m_what("Invalid XML file") {}
        [[nodiscard]] const char* what() const noexcept override {
            return m_what.c_str();
        }

       private:
        std::string m_what;
    };

    std::string_view get(const Strings& string) const override;
    [[nodiscard]] size_t size() const noexcept override {
        return m_data.size();
    }

   private:
    std::map<Strings, std::string> m_data;
};

class StringResLoader : public StringResLoaderBase {
    std::map<Locale, std::shared_ptr<LocaleStringsImpl>> localeMap;
    std::filesystem::path m_path;
    LocaleStringsImpl empty;

   public:
    // Load XML files from the specified path.
    explicit StringResLoader(std::filesystem::path path);
    ~StringResLoader() override;

    [[nodiscard]] size_t size() const noexcept override {
        return localeMap.size();
    }

    const LocaleStrings* at(const Locale key) const override;
};

inline std::string_view access(const StringResLoaderBase::LocaleStrings* locale,
                               Strings strings) {
    return locale->get(strings);
}