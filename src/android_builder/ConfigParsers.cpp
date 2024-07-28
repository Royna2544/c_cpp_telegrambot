#include "ConfigParsers.hpp"

#include <absl/log/log.h>
#include <internal/_class_helper_macros.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <algorithm>
#include <cstddef>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <source_location>
#include <type_traits>
#include <vector>

#include "CStringLifetime.h"
#include "TryParseStr.hpp"

constexpr bool parserDebug = false;

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

using st = std::source_location;
template <typename T>
using xmlCallback = const std::function<bool(T *)> &;

bool findChildNodes(xmlNode *parentNode, const char *name,
                    xmlCallback<xmlNode> &callback,
                    st location = st::current()) {
    if (parentNode == nullptr) {
        LOG(WARNING) << "Parent node is nullptr: " << location.file_name()
                     << ":" << location.line();
        return false;
    }
    if (parentNode->children == nullptr) {
        LOG(WARNING) << "Node is nullptr: " << location.file_name() << ":"
                     << location.line();
        return false;
    }

    // If name is not provided, just iterate over all children and return true
    for (xmlNode *currentNode = parentNode->children; currentNode != nullptr;
         currentNode = xmlNextElementSibling(currentNode)) {
        if (currentNode->type == XML_ELEMENT_NODE) {
            // If name is not provided...
            if (name == nullptr) {
                // IDC, we simply call the callback function
                callback(currentNode);
            } else {
                if (shim::xmlStrEq(currentNode->name, name)) {
                    if (!callback(currentNode)) {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

template <typename T>
struct Element;

// Provide type alias
using ParentElement = Element<std::nullptr_t>;
using StringElement = Element<std::string>;
using IntElement = Element<int>;
using StringArrayElement = Element<std::vector<std::string>>;
using IntArrayElement = Element<std::vector<int>>;

template <typename Cont, typename T>
void assign(Cont *cont, T value) = delete;

template <typename T>
void assign(std::vector<T> *cont, T &&value) {
    cont->emplace_back(std::forward<T>(value));
}

template <typename T>
void assign(T *cont, T &&value) {
    *cont = std::forward<std::decay_t<T>>(value);
}

template <typename T>
void assign(T *cont, const T &value) {
    *cont = value;
}

template <typename T>
void dump(const char *key, const T &value) {
    LOG(INFO) << key << ": " << value;
}

template <typename T>
void dump(const char *key, const std::vector<T> &value) {
    for (const auto &elem : value) {
        dump(key, elem);
    }
}

// Primary template (default case, not a specialization)
template <template <typename...> class Template, typename T>
struct is_specialization_of : std::false_type {};

// Partial specialization that matches specializations of Template
template <template <typename...> class Template, typename... Args>
struct is_specialization_of<Template, Template<Args...>> : std::true_type {};

// Helper variable template for convenience
template <template <typename...> class Template, typename T>
inline constexpr bool is_specialization_of_v =
    is_specialization_of<Template, T>::value;

// Primary template for ExtractType
template <typename T>
struct ExtractType {
    using Type = std::decay_t<T>;
};

// Specialization of ExtractType for std::vector<T>
template <typename T>
struct ExtractType<std::vector<T>> {
    using Type = std::decay_t<T>;
};

// Alias template for easier usage
template <typename T>
using ExtractType_t = typename ExtractType<T>::Type;

template <typename T>
class Element {
    T *out;
    bool has_value = false;
    bool using_default_value = false;

   public:
    using data_type = ExtractType_t<T>;
    using ElemVariant = std::variant<IntElement, StringElement,
                                     StringArrayElement, IntArrayElement>;

    const char *nodeName = nullptr;
    bool required = false;
    std::vector<ElemVariant> attrs;
    // Elemetname ElementType;
    std::vector<ElemVariant> childs;

    explicit Element(T *out, const char *nodeName)
        : out(out), nodeName(nodeName), required(true) {}

    explicit Element(T *out, const char *nodeName, T &&default_value)
        : out(out),
          nodeName(nodeName),
          has_value(true),
          using_default_value(true) {
        assign(out, std::move(default_value));
    }
    explicit Element(std::nullptr_t, const char *nodeName)
        : out(nullptr), nodeName(nodeName) {}

    bool usingDefault() const { return using_default_value; }

    bool parseAttrs(xmlNode *node) {
        if (attrs.empty()) {
            return true;
        }

        bool any = false;
        for (xmlAttr *xmlattr = node->properties; xmlattr != nullptr;
             xmlattr = xmlattr->next) {
            any = !any && std::ranges::any_of(attrs, [xmlattr](auto &elem) {
                return std::visit(
                    [xmlattr](auto &&attr) {
                        using V = std::decay_t<decltype(attr)>;
                        if constexpr (is_specialization_of_v<Element, V>) {
                            typename V::data_type t;
                            if (try_parse(
                                    shim::xmlNodeGetContent(xmlattr->children)
                                        .get(),
                                    &t)) {
                                attr.set(t);
                                return true;
                            }
                            return false;
                        }
                    },
                    elem);
            });
        }
        return any;
    }

    bool parseChilds(xmlNode *node) {
        if (childs.empty()) {
            return true;
        }

        return findChildNodes(node, nullptr, [this](xmlNode *xmlChild) {
            for (auto &childElem : childs) {
                std::visit(
                    [xmlChild](auto &&child) {
                        using V = std::decay_t<decltype(child)>;
                        if constexpr (is_specialization_of_v<Element, V>) {
                            if (!shim::xmlStrEq(xmlChild->name,
                                                child.nodeName)) {
                                return false;
                            }
                            return child.parse(xmlChild);
                        }
                    },
                    childElem);
            }
            return true;
        });
    }

    bool parse(xmlNode *node) {
        if constexpr (parserDebug) {
            if (!shim::xmlStrEq(node->name, nodeName)) {
                return false;
            }
        }

        // Parse attributes
        if (!parseAttrs(node)) {
            LOG(ERROR) << "Failed to parse attributes from: " << nodeName;
            return false;
        }
        // Parse children
        if (!parseChilds(node)) {
            LOG(ERROR) << "Failed to parse children from: " << nodeName;
            return false;
        }

        if constexpr (std::is_same_v<data_type, std::nullptr_t>) {
            // Nothing to do for nullptr
            return true;
        } else if constexpr (std::is_same_v<data_type, std::string>) {
            set(std::string(shim::xmlNodeGetContent(node).get()));
        } else if constexpr (std::is_integral_v<data_type>) {
            data_type value{};
            if (!try_parse(std::string(shim::xmlNodeGetContent(node).get()),
                           &value)) {
                LOG(ERROR) << "Failed to parse int from: "
                           << shim::xmlNodeGetContent(node);
                return false;
            }
            set(value);
        } else {
            LOG(WARNING) << "Unsupported type for parsing: " << typeid(T).name()
                         << ".";
            return false;
        }
        return true;
    }

    void set(data_type value) {
        has_value = true;
        using_default_value = false;
        assign(out, std::move(value));
    }

    void dump() const {
        if (has_value) {
            ::dump(nodeName, *out);
        } else {
            LOG(INFO) << nodeName << ": Not set";
        }
    }

    explicit operator bool() const { return has_value; }
};

template <typename... Elements>
bool testCompletion(const Elements &...elements) {
    return (
        [&elements] {
            if (elements) {
                if (elements.usingDefault()) {
                    LOG(WARNING)
                        << "Default value used for: " << elements.nodeName;
                }
                if constexpr (parserDebug) {
                    elements.dump();
                };
            } else if (elements.required) {
                LOG(ERROR) << "Missing required element: " << elements.nodeName;
                return false;
            }
            return true;
        }(),
        ...);
}

template <typename Type, typename... Elements>
    requires std::is_same_v<Type, xmlNode> || std::is_same_v<Type, xmlAttr>
bool parseElements(Type *node, Elements &...elements) {
    bool ret = false;

    ((ret = ret || elements.parse(node)), ...);

    return ret;
}

template <typename... Elements>
bool parseChildElements(xmlNode *parentNode, Elements &...elements) {
    return findChildNodes(parentNode, nullptr,
                          [&elements...](xmlNode *node) {
                              return parseElements(node, elements...);
                          }) &&
           testCompletion(elements...);
}

std::vector<ConfigParser::LocalManifest::Ptr> ConfigParser::Parser::parse()
    const {
    std::vector<ROMBranch::Ptr> branches;
    std::vector<LocalManifest::Ptr> manifests;
    findChildNodes(rootNode, "rom", [this, &branches](xmlNode *curNode) {
        auto rom = std::make_shared<ROMInfo>();
        std::vector<int> version;
        std::vector<std::string> branch;
        StringElement romName(&rom->name, "name");
        StringElement romLink(&rom->url, "link");
        StringElement romTarget(&rom->target, "target", "bacon");
        StringElement romOutzipPrefix(&rom->prefixOfOutput, "outzip_prefix");
        StringArrayElement android_and_branch(&branch, "branch");
        IntArrayElement versionElem(&version, "android_version");
        android_and_branch.attrs.emplace_back(std::move(versionElem));
        std::map<int, std::string> branchesMap;

        if (!parseChildElements(curNode, romName, romLink, romTarget,
                                romOutzipPrefix, android_and_branch)) {
            return false;
        }
        std::transform(version.begin(), version.end(), branch.begin(),
                       std::inserter(branchesMap, branchesMap.end()),
                       std::make_pair<int const &, std::string const &>);

        for (const auto &[k, v] : branchesMap) {
            branches.emplace_back(
                std::make_shared<ROMBranch>(v, k, std::string(), rom));
        }
        return true;
    });
    if (branches.empty()) {
        LOG(ERROR) << "Failed to parse rom config";
        return {};
    }

    findChildNodes(rootNode, "local_manifests", [&, this](xmlNode *root) {
        std::string name;
        std::string url;
        std::vector<LocalManifest::Ptr> localManifests;
        StringElement localManifestName(&name, "name");
        StringElement localManifestLink(&url, "url");

        bool ret =
            parseChildElements(root, localManifestName, localManifestLink);
        if (!ret) {
            return false;
        }

        findChildNodes(root, "branch", [&, this](xmlNode *curNode) {
            auto locMan = std::make_shared<LocalManifest>();
            std::string targetrom;
            int androidVersion = 0;
            std::string device;

            ParentElement parent(nullptr, "devices");
            StringElement deviceElem(&device, "device");
            StringElement targetRomElem(&targetrom, "target_rom");
            IntElement androidVersionElem(&androidVersion, "android_version");

            parent.childs.emplace_back(
                StringArrayElement(&locMan->devices, "codename"));

            deviceElem.required = false;
            if (!parseChildElements(curNode, deviceElem, parent,
                                    targetRomElem, androidVersionElem)) {
                return false;
            }
            locMan->repo_info.url = url;
            locMan->name = name;
            if (deviceElem) {
                locMan->devices.emplace_back(device);
            }

            // If it's '*', we append all known roms here
            if (targetrom == "*") {
                for (const auto &rom : branches) {
                    locMan->rom = rom;
                    localManifests.emplace_back(locMan);
                }
            } else {
                // Lookup the ROM by android version and rom name
                for (const auto &rom : branches) {
                    if (rom->androidVersion == androidVersion &&
                        rom->romInfo->name == targetrom) {
                        locMan->rom = rom;
                        break;
                    }
                }
                if (!locMan->rom) {
                    LOG(WARNING)
                        << "No rom found for android version " << androidVersion
                        << " and rom name " << targetrom << ", abort";
                    return false;
                }

                localManifests.emplace_back(locMan);
            }
            return true;
        });
        manifests = localManifests;
        return true;
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