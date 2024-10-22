#include "StringResLoader.hpp"

#include <absl/log/log.h>
#include <absl/strings/str_split.h>
#include <fmt/format.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/valid.h>

#include <array>
#include <filesystem>
#include <vector>

struct libxml2_error_ctx {
    int code;
    std::string message;
};

namespace {
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
}  // namespace

bool StringResLoader::parse(const std::filesystem::path &path,
                            int expected_size) {
    // Initialize the library and check potential ABI mismatches
    LIBXML_TEST_VERSION;
    xmlChar resourceKey[] = "resources";
    xmlChar stringKey[] = "string";
    xmlChar nameProp[] = "name";

    libxml2_error_ctx ctx;

    // Set up error handling
    xmlSetGenericErrorFunc(&ctx, libxml_error_handler);

    // Parse the XML file
    xmlDocPtr doc = xmlReadFile(path.string().c_str(), nullptr, 0);
    if (doc == nullptr) {
        LOG(ERROR) << fmt::format("Could not parse file {} (code: {})",
                                  path.string(), ctx.code);
        std::vector<std::string> errors = absl::StrSplit(ctx.message, '\n', absl::SkipEmpty());
        for (const auto &error : errors) {
            LOG(ERROR) << "libxml2 messages: " << error;
        }
        return false;
    }

    LOG(INFO) << "Parsing file: " << path;

    // Get the root element node
    xmlNodePtr rootElement = xmlDocGetRootElement(doc);

    // Ensure the root element is <resources>
    if (xmlStrcmp(rootElement->name, resourceKey) != 0) {
        LOG(ERROR) << "Root element is not <resources>";
        xmlFreeDoc(doc);
        return false;
    }

    // Iterate through <string> elements
    for (xmlNodePtr cur = rootElement->children; cur != nullptr;
         cur = cur->next) {
        if (cur->type == XML_ELEMENT_NODE &&
            (xmlStrcmp(cur->name, stringKey) == 0)) {
            xmlChar *nameAttr = xmlGetProp(cur, nameProp);
            xmlChar *content = xmlNodeListGetString(doc, cur->children, 1);

            if ((nameAttr != nullptr) && (content != nullptr)) {
                m_strings.emplace_back(reinterpret_cast<char *>(nameAttr),
                                       reinterpret_cast<char *>(content));
            }

            if (nameAttr != nullptr) {
                xmlFree(nameAttr);
            }
            if (content != nullptr) {
                xmlFree(content);
            }
        }
    }

    // Sort the strings by name
    std::ranges::sort(m_strings, [](auto &&lfs, auto &&rfs) {
        return lfs.first > rfs.first;
    });

    // Free the document
    xmlFreeDoc(doc);

    // Cleanup function for the XML library
    xmlCleanupParser();

    if (expected_size > 0 && m_strings.size() != expected_size) {
        LOG(ERROR) << "Number of strings(" << m_strings.size()
                   << ") is not equal to expected_size(" << expected_size
                   << ")";
        return false;
    }
    return true;
}

std::string_view StringResLoader::getString(const int key) const {
    if (key > m_strings.size() || key < 0) {
        LOG(WARNING) << "Invalid key: " << key;
        return "";
    }
    return m_strings.at(key).second;
}