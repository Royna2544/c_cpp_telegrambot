#pragma once

#include <Localization.hpp>
#include <exception>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <trivial_helpers/fruit_inject.hpp>
#include <unordered_map>

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
    std::unordered_map<Strings, std::string> m_data;
};

class StringResLoader : public StringResLoaderBase {
    std::unordered_map<Locale, std::shared_ptr<LocaleStringsImpl>> localeMap;
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