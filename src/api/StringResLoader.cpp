#include <AbslLogCompat.hpp>
#include <absl/strings/ascii.h>
#include <absl/strings/str_split.h>
#include <fmt/format.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/valid.h>

#include <api/StringResLoader.hpp>
#include <array>
#include <filesystem>
#include <libxml2_helper.hpp>
#include <memory>
#include <string_view>
#include <system_error>
#include <trivial_helpers/raii.hpp>
#include <utility>

struct LocaleResData {
    std::string name;
    bool isDefault;
};

using invalid_xml_error = std::exception;

namespace {

Strings getStrings(const std::string_view string) {
#define fn(x) \
    if (string == #x) return Strings::x;
    MAKE_STRINGS(fn);
#undef fn
    return {};
}
std::string_view getStrings(const Strings string) {
    switch (string) {
#define fn(x)        \
    case Strings::x: \
        return #x;
        MAKE_STRINGS(fn);
#undef fn
    }
    return "";
}

std::pair<LocaleResData, StringResLoader::PerLocaleMapImpl> parseLocaleResource(
    const std::filesystem::path &filename) {
    auto resourceKey = "resources"_xmlChar;
    auto localeProp = "locale"_xmlChar;
    auto defaultProp = "default"_xmlChar;
    auto stringKey = "string"_xmlChar;
    auto nameProp = "name"_xmlChar;
    constexpr auto STRINGRES_MAX = static_cast<int>(Strings::__MAX__);
    constexpr auto STRINGRES_MIN = static_cast<int>(Strings::__INVALID__) + 1;

    StringResLoader::PerLocaleMapImpl m_data;

    libxml2_error_ctx ctx;
    // Set up error handling
    xmlSetGenericErrorFunc(&ctx, libxml2_error_handler);

    // Parse the XML file
    auto doc = RAII<xmlDocPtr>::template create<void>(
        xmlReadFile(filename.string().c_str(), nullptr, 0), xmlFreeDoc);
    if (doc == nullptr) {
        LOG(ERROR) << fmt::format("Could not parse file {} (code: {})",
                                  filename.string(), ctx.code);
        std::vector<std::string> errors =
            absl::StrSplit(ctx.message, '\n', absl::SkipEmpty());
        for (const auto &error : errors) {
            LOG(ERROR) << "libxml2 messages: " << error;
        };
        throw invalid_xml_error();
    }

    // Get the root element node
    xmlNodePtr rootElement = xmlDocGetRootElement(doc.get());

    // Ensure the root element is <resources>
    if (xmlStrcmp(rootElement->name, resourceKey) != 0) {
        LOG(ERROR) << "Root element is not <resources>";
        throw invalid_xml_error();
    }

    XmlCharWrapper localeAttr = xmlGetProp(rootElement, localeProp);
    if (!localeAttr) {
        LOG(ERROR) << "No locale= key in <resources>";
        throw invalid_xml_error();
    }

    XmlCharWrapper defaultAttr = xmlGetProp(rootElement, defaultProp);
    bool isDefault = false;

    if (defaultAttr == "yes") {
        isDefault = true;
    }

    m_data.reserve(STRINGRES_MAX);
    for (xmlNodePtr cur = rootElement->children; cur != nullptr;
         cur = cur->next) {
        if (cur->type == XML_ELEMENT_NODE && cur->name == stringKey) {
            XmlCharWrapper nameAttr = xmlGetProp(cur, nameProp);
            XmlCharWrapper content =
                xmlNodeListGetString(doc.get(), cur->children, 1);

            if (!nameAttr || !content) {
                continue;
            }
	    auto upper = absl::AsciiStrToUpper(nameAttr.c_str());
            auto string = getStrings(upper);
            if (string == Strings::__INVALID__) {
                LOG(WARNING) << "String name not in enumerator: " << upper;
                continue;
            }
            auto [it, res] = m_data.emplace(string, content.str());
            LOG_IF(WARNING, !res) << "Failed to emplace string " << string;
        }
    }
    int absent = 0;
    for (int x = STRINGRES_MIN; x < STRINGRES_MAX; ++x) {
        if (!m_data.contains(static_cast<Strings>(x))) {
            LOG(WARNING) << "Missing string: "
                         << getStrings(static_cast<Strings>(x));
            ++absent;
        }
    }
    if (absent != 0) {
        LOG(ERROR) << "Incomplete XML file. Missing strings: " << absent;
        throw invalid_xml_error();
    }
    return {LocaleResData{localeAttr.str(), isDefault}, std::move(m_data)};
}

}  // namespace

StringResLoader::StringResLoader(std::filesystem::path path)
    : m_path(std::move(path)) {
    // Initialize the library and check potential ABI mismatches
    LIBXML_TEST_VERSION;

    std::error_code ec;
    // Load XML files from the specified path
    for (const auto &entry : std::filesystem::directory_iterator(m_path, ec)) {
        bool isXml = entry.is_regular_file() &&
                     entry.path().filename().extension() == ".xml";
        if (!isXml) continue;

        try {
            auto [locale, map] = parseLocaleResource(entry.path().string());
            localeMap[locale.name] = std::move(map);
            if (locale.isDefault) {
                default_locale = locale.name;
            }
            LOG(INFO) << "Parsed locale: " << locale.name
                      << " File: " << entry.path().filename()
                      << " isDefault: " << locale.isDefault;
        } catch (const invalid_xml_error &e) {
            LOG(ERROR) << "Invalid XML file: " << entry.path();
            continue;
        }
    }
    if (ec) {
        LOG(ERROR) << "Error reading directory: " << ec.message();
    }
    if (!default_locale) {
        LOG(ERROR) << "No default locale set";
        // TODO: What I can do in this case?
    }
}

StringResLoader::~StringResLoader() { xmlCleanupParser(); }

const StringResLoader::PerLocaleMap *StringResLoader::at(
    const std::string &key) const {
    if (localeMap.contains(key)) {
        return &localeMap.at(key);
    } else if (default_locale) {
        DLOG_IF(WARNING, !key.empty()) << "Locale not found: " << key;
        return &localeMap.at(*default_locale);
    } else [[unlikely]] {
        LOG(ERROR) << "StringResLoader::at has nothing to return!";
        return nullptr; // Intended crash
    }
}
