#include <absl/log/log.h>
#include <absl/strings/ascii.h>
#include <absl/strings/str_split.h>
#include <fmt/format.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/valid.h>

#include <api/StringResLoader.hpp>
#include <array>
#include <filesystem>
#include <memory>
#include <string_view>
#include <system_error>
#include <trivial_helpers/raii.hpp>
#include <utility>

struct libxml2_error_ctx {
    int code{};
    std::string message;
};

struct LocaleResData {
    std::string name;
    bool isDefault;
};

struct XmlCharWrapper {
    RAII<xmlChar *>::Value<void> string;
    operator ::xmlChar *() const { return string.get(); }
};

XmlCharWrapper operator""_xmlChar(const char *string, size_t length) {
    return {RAII<xmlChar *>::create<void>(xmlCharStrdup(string), xmlFree)};
}

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

void libxml_error_handler(void *ctx, const char *msg, ...) {
    va_list args;
    std::array<char, 256> errorBuffer{};
    va_start(args, msg);
    vsnprintf(errorBuffer.data(), errorBuffer.size(), msg, args);
    va_end(args);
    auto *error_ctx = static_cast<libxml2_error_ctx *>(ctx);
    error_ctx->code = xmlGetLastError()->code;
    error_ctx->message += errorBuffer.data();
}

std::pair<LocaleResData, StringResLoader::PerLocaleMapImpl> parseLocaleResource(
    const std::filesystem::path &filename) {
    auto resourceKey = "resources"_xmlChar;
    auto localeProp = "locale"_xmlChar;
    auto defaultProp = "default"_xmlChar;
    auto stringKey = "string"_xmlChar;
    auto nameProp = "name"_xmlChar;

    StringResLoader::PerLocaleMapImpl m_data;

    libxml2_error_ctx ctx;
    // Set up error handling
    xmlSetGenericErrorFunc(&ctx, libxml_error_handler);

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

    auto localeAttr = RAII<xmlChar *>::template create<void>(
        xmlGetProp(rootElement, localeProp), xmlFree);
    if (!localeAttr) {
        LOG(ERROR) << "No locale= key in <resources>";
        throw invalid_xml_error();
    }

    auto defaultAttr = RAII<xmlChar *>::template create<void>(
        xmlGetProp(rootElement, defaultProp), xmlFree);
    bool isDefault = false;

    if (defaultAttr && xmlStrcmp(defaultAttr.get(), "yes"_xmlChar) == 0) {
        isDefault = true;
    }

    m_data.reserve(static_cast<int>(Strings::__MAX__));
    for (xmlNodePtr cur = rootElement->children; cur != nullptr;
         cur = cur->next) {
        if (cur->type == XML_ELEMENT_NODE &&
            xmlStrcmp(cur->name, stringKey) == 0) {
            auto nameAttr = RAII<xmlChar *>::template create<void>(
                xmlGetProp(cur, nameProp), xmlFree);
            auto content = RAII<xmlChar *>::template create<void>(
                xmlNodeListGetString(doc.get(), cur->children, 1), xmlFree);

            if (nameAttr && content) {
                auto upper = absl::AsciiStrToUpper(
                    reinterpret_cast<const char *>(nameAttr.get()));
                auto string = getStrings(upper);
                if (string == Strings::__INVALID__) {
                    LOG(WARNING) << "String name not in enumerator: " << upper;
                    continue;
                }
                m_data.emplace(string,
                               reinterpret_cast<const char *>(content.get()));
            }
        }
    }
    int absent = 0;
    for (int x = static_cast<int>(Strings::__INVALID__) + 1;
         x < static_cast<int>(Strings::__MAX__); ++x) {
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
    return {LocaleResData{reinterpret_cast<const char *>(localeAttr.get()),
                          isDefault},
            std::move(m_data)};
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
                default_map = &localeMap[locale.name];
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
    if (!default_map) {
        LOG(ERROR) << "No default locale set";
        // TODO: What I can do in this case?
    }
}

StringResLoader::~StringResLoader() { xmlCleanupParser(); }

const StringResLoader::PerLocaleMap *StringResLoader::at(
    const std::string &key) const {
    if (localeMap.contains(key)) {
        return &localeMap.at(key);
    } else {
        DLOG(WARNING) << "Locale not found: " << key;
        return default_map;
    }
}
