#pragma once

#include <absl/log/log.h>
#include <libxml/tree.h>

#include <string>

#include "RepoUtils.hpp"

class ConfigParser {
   public:
    struct ROMInfo {
        using Ptr = std::shared_ptr<ROMInfo>;

        std::string name;            // name of the ROM
        std::string url;             // URL of the ROM repo
        std::string target;          // build target to build a ROM
        std::string prefixOfOutput;  // prefix of output zip file
    };

    struct ROMBranch {
        using Ptr = std::shared_ptr<ROMBranch>;

        std::string branch;          // branch of the repo
        int androidVersion;          // android version of the branch
        std::string prefixOfOutput;  // prefix of output zip file
        ROMInfo::Ptr romInfo;        // associated ROMInfo
    };

    struct LocalManifest {
        using Ptr = std::shared_ptr<LocalManifest>;

        std::string name;                  // name of the manifest
        ROMBranch::Ptr rom;                // associated ROM and its branch
        RepoUtils::RepoInfo repo_info;     // local manifest information
        std::vector<std::string> devices;  // codename of the device
    };

    explicit ConfigParser(const std::filesystem::path &xmlFilePath);

    struct ROMEntry {
        std::string romName;         // name of the ROM
        int androidVersion;          // android version of the ROM
        const ConfigParser *parser;  // pointer to the ConfigParser instance

        LocalManifest::Ptr getLocalManifest() const;
    };

    struct DeviceEntry {
        std::string device;          // device codename
        const ConfigParser *parser;  // pointer to the ConfigParser instance

        [[nodiscard]] std::vector<ROMEntry> getROMs() const;
    };
    [[nodiscard]] std::vector<DeviceEntry> getDevices() const;

   private:
    std::vector<LocalManifest::Ptr> parsedManifests;

    class Parser {
        xmlNode *rootNode;
        xmlDoc *doc;

       public:
        explicit Parser(const std::filesystem::path &xmlFilePath);
        ~Parser();
        [[nodiscard]] std::vector<LocalManifest::Ptr> parse() const;
    };
};

struct PerBuildData {
    std::string device;  // device codename
    ConfigParser::LocalManifest::Ptr
        localManifest;  // associated local manifest
    enum class Variant {
        kUser,
        kUserDebug,
        kEng
    } variant;  // Target build variant

    enum class Result { SUCCESS, ERROR_NONFATAL, ERROR_FATAL };
    struct ResultData {
        static constexpr int MSG_SIZE = 250;
        Result value{};
        std::array<char, MSG_SIZE> msg{};
        void setMessage(const std::string &message) {
            LOG_IF(WARNING, message.size() > msg.size())
                << "Message size is " << message.size()
                << " bytes, which exceeds limit";
            std::strncpy(msg.data(), message.c_str(), msg.size() - 1);
        }
        [[nodiscard]] std::string getMessage() const noexcept {
            return msg.data();
        }
    } *result;
};