#include "ConfigParsers.hpp"

#include <absl/log/log.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <algorithm>
#include <bitset>
#include <functional>
#include <memory>
#include <source_location>
#include <unordered_map>
#include <vector>

#include "CStringLifetime.h"
#include "TryParseStr.hpp"

constexpr bool parserDebug = true;

namespace shim {
bool xmlStrEq(const xmlChar *a, const char *b) {
    return xmlStrEqual(a, (const xmlChar *)b) != 0;
}

CStringLifetime xmlNodeGetContent(xmlNode *node) {
    xmlChar *content = ::xmlNodeGetContent(node);
    if (content != nullptr) {
        std::string contentStr(reinterpret_cast<const char *>(content));
        xmlFree(content);
        return contentStr;
    }
    LOG(ERROR) << "Failed to get content of node: " << node->name;
    return "";
}
}  // namespace shim

namespace {
void printKV(const std::string &key, const std::string &value) {
    if constexpr (parserDebug) {
        LOG(INFO) << key << ": " << value;
    }
}

using st = std::source_location;
template <typename T>
using xmlCallback = const std::function<void(T *)> &;

void for_each_xmlnode(xmlNode *node, xmlCallback<xmlNode> callback,
                      st location = st::current()) {
    if (node == nullptr) {
        LOG(WARNING) << "Node is nullptr: " << location.file_name() << ":"
                     << location.line();
        return;
    }

    for (xmlNode *currentNode = node; currentNode != nullptr;
         currentNode = xmlNextElementSibling(currentNode)) {
        if (currentNode->type == XML_ELEMENT_NODE) {
            callback(currentNode);
        }
    }
}

void for_each_child_xmlnode(xmlNode *parentNode, xmlCallback<xmlNode> &callback,
                            st location = st::current()) {
    if (parentNode == nullptr) {
        LOG(WARNING) << "Parent node is nullptr: " << location.file_name()
                     << ":" << location.line();
        return;
    }

    for_each_xmlnode(parentNode->children, callback);
}

// Function to iterate over all attributes of a given XML node and apply a
// callback function
void for_each_xmlattr_fromnode(xmlNode *node, xmlCallback<xmlAttr> &callback,
                               st location = st::current()) {
    if (node == nullptr) {
        LOG(WARNING) << "Node is nullptr: " << location.file_name() << ":"
                     << location.line();
        return;
    }
    if (node->properties == nullptr) {
        LOG(WARNING) << "Node properties are nullptr";
        return;
    }
    for (xmlAttr *attr = node->properties; attr != nullptr; attr = attr->next) {
        callback(attr);
    }
}
}  // namespace

using ParserBitset = std::bitset<4>;
// Common
constexpr ParserBitset kHasName = 1 << 0;
// For ROM parse
constexpr ParserBitset kHasLink = 1 << 1;
constexpr ParserBitset kHasTarget = 1 << 2;
constexpr ParserBitset kHasOutzipPrefix = 1 << 3;
// For localmanifest-branch parse
constexpr ParserBitset kHasTargetROM = 1 << 1;
constexpr ParserBitset kHasAndroidVersion = 1 << 2;
constexpr ParserBitset kHasDevice = 1 << 3;
// For localmanifest parse
constexpr ParserBitset kHasUrl = 1 << 1;

std::vector<ConfigParser::LocalManifest::Ptr> ConfigParser::Parser::parse()
    const {
    std::vector<ROMBranch::Ptr> branches;
    std::vector<LocalManifest::Ptr> manifests;
    for_each_child_xmlnode(rootNode, [this, &branches](xmlNode *curNode) {
        if (shim::xmlStrEq(curNode->name, "rom")) {
            auto rom = std::make_shared<ROMInfo>();
            ParserBitset bitset;
            std::unordered_map<int, std::string> android_and_branch;

            for_each_child_xmlnode(
                curNode, [&rom, &bitset, &android_and_branch](xmlNode *node) {
                    if (shim::xmlStrEq(node->name, "name")) {
                        rom->name = shim::xmlNodeGetContent(node);
                        bitset |= kHasName;
                    } else if (shim::xmlStrEq(node->name, "link")) {
                        rom->url = shim::xmlNodeGetContent(node);
                        bitset |= kHasLink;
                    } else if (shim::xmlStrEq(node->name, "target")) {
                        rom->target = shim::xmlNodeGetContent(node);
                        bitset |= kHasTarget;
                    } else if (shim::xmlStrEq(node->name, "outzip_prefix")) {
                        rom->prefixOfOutput = shim::xmlNodeGetContent(node);
                        bitset |= kHasOutzipPrefix;
                    } else if (shim::xmlStrEq(node->name, "branch")) {
                        for_each_xmlattr_fromnode(node, [&android_and_branch,
                                                         node](xmlAttr *attr) {
                            if (shim::xmlStrEq(attr->name, "android_version")) {
                                int version = 0;
                                if (try_parse(
                                        shim::xmlNodeGetContent(attr->children)
                                            .get(),
                                        &version)) {
                                    android_and_branch[version] =
                                        shim::xmlNodeGetContent(node);
                                } else {
                                    LOG(ERROR) << "Failed to parse android "
                                                  "version from: "
                                               << shim::xmlNodeGetContent(
                                                      attr->children);
                                }
                            }
                        });
                    }
                });
            if (!(bitset & kHasName).any()) {
                LOG(ERROR) << "No name provided";
            }

            if (!(bitset & kHasLink).any()) {
                LOG(ERROR) << "No link provided";
            }

            if (!(bitset & kHasTarget).any()) {
                LOG(WARNING) << "No target provided, assuming bacon";
                rom->target = "bacon";
                bitset |= kHasTarget;
            }
            if (!(bitset & kHasOutzipPrefix).any()) {
                LOG(ERROR) << "No outzip prefix provided";
            }

            if (android_and_branch.empty()) {
                LOG(ERROR) << "No branch provided";
            }

            if (!bitset.all()) {
                LOG(ERROR) << "Done.";
                branches.clear();
                return;
            }

            if constexpr (parserDebug) {
                printKV("Name", rom->name);
                printKV("Link", rom->url);
                printKV("Target", rom->target);
                printKV("Outzip Prefix", rom->prefixOfOutput);
                for (const auto &pair : android_and_branch) {
                    printKV("Android version", std::to_string(pair.first));
                    printKV("Branch", pair.second);
                }
            }

            for (const auto &[k, v] : android_and_branch) {
                branches.emplace_back(
                    std::make_shared<ROMBranch>(v, k, std::string(), rom));
            }
        }
    });
    if (branches.empty()) {
        LOG(ERROR) << "Failed to parse rom config";
        return {};
    }

    for_each_child_xmlnode(rootNode, [&, this](xmlNode *root) {
        if (shim::xmlStrEq(root->name, "local_manifests")) {
            std::string name;
            std::string url;
            ParserBitset bitset;
            std::vector<LocalManifest::Ptr> localManifests;
            for_each_child_xmlnode(
                root, [&name, &bitset, &url](xmlNode *curNode) {
                    if (shim::xmlStrEq(curNode->name, "name")) {
                        name = shim::xmlNodeGetContent(curNode);
                        bitset |= kHasName;
                    } else if (shim::xmlStrEq(curNode->name, "url")) {
                        url = shim::xmlNodeGetContent(curNode);
                        bitset |= kHasUrl;
                    }
                });
            if (!(bitset & kHasName).any()) {
                LOG(ERROR) << "No name provided";
            }
            if (!(bitset & kHasUrl).any()) {
                LOG(ERROR) << "No url provided";
            }
            if (!bitset.test(0) || !bitset.test(1)) {
                LOG(ERROR) << "Done.";
                return;
            }
            if constexpr (parserDebug) {
                printKV("Name", name);
                printKV("Url", url);
            }
            for_each_child_xmlnode(root, [&, this](xmlNode *curNode) {
                if (shim::xmlStrEq(curNode->name, "branch")) {
                    auto locMan = std::make_shared<LocalManifest>();
                    ParserBitset bitset;
                    std::pair<std::string, int> android_and_rom;
                    for_each_child_xmlnode(curNode, [&, this](xmlNode *attr) {
                        if (shim::xmlStrEq(attr->name, "name")) {
                            locMan->repo_info.branch =
                                shim::xmlNodeGetContent(attr->children);
                            bitset |= kHasName;
                        } else if (shim::xmlStrEq(attr->name, "target_rom")) {
                            if (shim::xmlNodeGetContent(attr->children).get() ==
                                std::string("*")) {
                                locMan->rom = anyROMObject();
                                android_and_rom.first = "any";
                            } else {
                                android_and_rom.first =
                                    shim::xmlNodeGetContent(attr->children);
                            }
                            bitset |= kHasTargetROM;
                        } else if (shim::xmlStrEq(attr->name,
                                                  "android_version")) {
                            int version = 0;
                            if (try_parse(
                                    shim::xmlNodeGetContent(attr->children)
                                        .get(),
                                    &version)) {
                                android_and_rom.second = version;
                                bitset |= kHasAndroidVersion;
                            } else {
                                LOG(ERROR)
                                    << "Failed to parse android "
                                       "version from: "
                                    << shim::xmlNodeGetContent(attr->children);
                                return;
                            }

                        } else if (shim::xmlStrEq(attr->name, "devices")) {
                            for_each_child_xmlnode(attr, [&bitset, &locMan](
                                                             xmlNode *child) {
                                if (shim::xmlStrEq(child->name, "codename")) {
                                    locMan->devices.emplace_back(
                                        shim::xmlNodeGetContent(
                                            child->children));
                                    bitset |= kHasDevice;
                                }
                            });

                        } else if (shim::xmlStrEq(attr->name, "device")) {
                            locMan->devices.emplace_back(
                                shim::xmlNodeGetContent(attr));
                            bitset |= kHasDevice;
                        }
                    });
                    if (!(bitset & kHasTargetROM).any()) {
                        LOG(ERROR) << "No target rom provided";
                    }

                    if (!(bitset & kHasAndroidVersion).any()) {
                        LOG(ERROR) << "No android version provided";
                    }

                    if (!(bitset & kHasDevice).any()) {
                        LOG(ERROR) << "No device provided";
                    }
                    if constexpr (parserDebug) {
                        printKV("Target ROM", android_and_rom.first);
                        printKV("Android Version",
                                std::to_string(android_and_rom.second));
                        for (const auto &device : locMan->devices) {
                            printKV("Device", device);
                        }
                    }

                    if (!bitset.all()) {
                        LOG(ERROR) << "Done.";
                        return;
                    }
                    locMan->repo_info.url = url;
                    locMan->name = name;

                    // When it's not 'all'
                    if (!locMan->rom) {
                        // Lookup the ROM by android version and rom name
                        for (const auto &rom : branches) {
                            if (rom->androidVersion == android_and_rom.second &&
                                rom->romInfo->name == android_and_rom.first) {
                                if constexpr (parserDebug) {
                                    printKV("Selected Rom Name",
                                            rom->romInfo->name);
                                    printKV("Selected Branch", rom->branch);
                                }
                                locMan->rom = rom;
                                break;
                            }
                        }
                        if (!locMan->rom) {
                            LOG(WARNING)
                                << "No rom found for android version "
                                << android_and_rom.second << " and rom name "
                                << android_and_rom.first << ", abort";
                            return;
                        }

                        localManifests.emplace_back(locMan);
                    } else {
                        // else, we append all known roms here
                        for (const auto &rom : branches) {
                            locMan->rom = rom;
                            localManifests.emplace_back(locMan);
                        }
                    }
                }
            });
            manifests = localManifests;
        }
    });

    return manifests;
}

ConfigParser::Parser::Parser(const std::filesystem::path &xmlFilePath)
    : doc(xmlReadFile(xmlFilePath.c_str(), nullptr, 0)) {
    if (doc == nullptr) {
        LOG(ERROR) << "Could not parse file: " << xmlFilePath.string();
        throw std::runtime_error("Could not parse XML file");
    }
    rootNode = xmlDocGetRootElement(doc);
    if (rootNode == nullptr) {
        LOG(ERROR) << "Empty XML document";
    }
}

ConfigParser::Parser::~Parser() {
    xmlFreeDoc(doc);
    xmlCleanupParser();
}

ConfigParser::ConfigParser(const std::filesystem::path &xmlFilePath) {
    Parser parser(xmlFilePath);
    parsedManifests = parser.parse();
}

std::vector<ConfigParser::DeviceEntry> ConfigParser::getDevices() const {
    std::vector<DeviceEntry> devices;
    for (const auto &manifest : parsedManifests) {
        for (const auto &device : manifest->devices) {
            if (std::ranges::find_if(devices, [&device](auto &&d) {
                    return d.device == device;
                }) == devices.end()) {
                devices.emplace_back(device, this);
            }
        }
    }
    return devices;
}

std::vector<ConfigParser::ROMEntry> ConfigParser::DeviceEntry::getROMs() const {
    std::vector<ROMEntry> roms;
    std::vector<ROMBranch::Ptr> romsInfo;

    // Sort out relevants
    std::ranges::for_each(
        parser->parsedManifests,
        [this, &romsInfo](const LocalManifest::Ptr &manifest) {
            bool x = std::ranges::find_if(manifest->devices,
                                          [this](const std::string &device) {
                                              return device == this->device;
                                          }) != manifest->devices.end();
            if (x) {
                romsInfo.emplace_back(manifest->rom);
            }
        });

    // Sort out ROMInfo::Ptr only
    std::ranges::transform(
        parser->parsedManifests, std::back_inserter(romsInfo),
        [](const LocalManifest::Ptr &manifest) { return manifest->rom; });
    // Remove duplicates
    const auto dels = std::ranges::unique(romsInfo);
    romsInfo.erase(dels.begin(), dels.end());

    // Map them to names
    std::ranges::transform(romsInfo, std::back_inserter(roms),
                           [this](const ROMBranch::Ptr &branch) {
                               return ROMEntry(branch->romInfo->name,
                                               branch->androidVersion, parser);
                           });

    // Done.
    return roms;
}

ConfigParser::LocalManifest::Ptr ConfigParser::ROMEntry::getLocalManifest()
    const {
    for (const auto &manifest : parser->parsedManifests) {
        if (manifest->rom->romInfo->name == romName &&
            manifest->rom->androidVersion == androidVersion) {
            return manifest;
        }
    }
    return nullptr;
}