#include "StringResLoader.hpp"

#include <CStringLifetime.h>
#include <absl/log/log.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <filesystem>
#include <vector>

#include "InstanceClassBase.hpp"

bool StringResLoader::parseFromFile(const std::filesystem::path &path,
                                     int expected_size) {
    // Initialize the library and check potential ABI mismatches
    LIBXML_TEST_VERSION;
    CStringLifetime filename = path.string().c_str();
    xmlChar resourceKey[] = "resources";
    xmlChar stringKey[] = "string";
    xmlChar nameProp[] = "name";
    // Parse the XML file
    xmlDocPtr doc = xmlReadFile(filename, nullptr, 0);
    if (doc == nullptr) {
        LOG(ERROR) << "Could not parse file " << filename;
        return false;
    }

    LOG(INFO) << "Parsing file: " << filename;

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

std::string StringResLoader::getString(const int key) const {
    return m_strings.at(key).second;
}