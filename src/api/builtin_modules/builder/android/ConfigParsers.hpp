#pragma once

#include <absl/log/log.h>
#include <fmt/format.h>

#include <filesystem>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>

struct RepoInfo {
    std::string url;
    std::string branch;
};

class ConfigParser {
   public:
    struct ROMInfo {
        using Ptr = std::shared_ptr<ROMInfo>;

        std::string name;    // name of the ROM
        std::string url;     // URL of the ROM repo
        std::string target;  // build target to build a ROM
        struct Artifact {
            std::string matcher;
            std::string data;
        } artifact;

        bool operator==(const ROMInfo& other) const = default;
    };

    struct AndroidVersion {
        using Ptr = std::shared_ptr<AndroidVersion>;
        float version;
        std::string name;
    };

    struct ROMBranch {
        using Ptr = std::shared_ptr<ROMBranch>;

        std::string branch;                  // branch of the repo
        AndroidVersion::Ptr androidVersion;  // android version of the branch

        ROMInfo::Ptr romInfo;  // associated ROMInfo

        // Returns a string representation of the ROM branch
        [[nodiscard]] static std::string makeKey(std::string_view name,
                                                 const float androidVersion) {
            return fmt::format("{}-{}", name, androidVersion);
        }
        [[nodiscard]] std::string makeKey() const {
            if (!romInfo) {
                throw std::logic_error("No rom info available");
            }
            return makeKey(romInfo->name, androidVersion->version);
        }
    };

    struct Device {
        using Ptr = std::shared_ptr<Device>;

        std::string codename;    // codename of the device (e.g. raven)
        std::string marketName;  // market name of the device (e.g. Pixel 5 Pro)

        Device() = default;
        Device(std::string codename, std::string marketName)
            : codename(std::move(codename)),
              marketName(std::move(marketName)) {}
        explicit Device(std::string codename) : codename(std::move(codename)) {}

        // Returns a string representation of the device
        // e.g. Pixel 5 Pro (raven)
        [[nodiscard]] std::string toString() const {
            if (marketName.empty()) {
                return codename;
            }
            return marketName + " (" + codename + ")";
        }

        bool operator==(const Device& other) const {
            return codename == other.codename;
        }
    };

    struct LocalManifest {
        using Ptr = std::shared_ptr<LocalManifest>;
        // name of the manifest
        std::string name;
        // associated ROM and its branch
        ROMBranch::Ptr rom;
        // associated devices
        std::vector<Device::Ptr> devices;
    };

    explicit ConfigParser(std::filesystem::path jsonFileDir);

    bool merge();

    // Get available devices from config
    [[nodiscard]] std::vector<ConfigParser::LocalManifest::Ptr> manifests()
        const;

   private:
    std::vector<LocalManifest::Ptr> parsedManifests;
    std::unordered_map<std::string, Device::Ptr> deviceMap;
    std::unordered_map<std::string, ROMBranch::Ptr> romBranchMap;
    std::unordered_map<float, AndroidVersion::Ptr> androidVersionMap;
    std::filesystem::path _jsonFileDir;
};

struct PerBuildData {
    // device codename
    ConfigParser::Device::Ptr device;
    // associated local manifest
    ConfigParser::LocalManifest::Ptr localManifest;
    enum class Variant {
        kUser,
        kUserDebug,
        kEng
    } variant;  // Target build variant

    enum class Result { NONE, SUCCESS, ERROR_NONFATAL, ERROR_FATAL };

    void reset() {
        device.reset();
        localManifest.reset();
        variant = Variant::kUser;
        result = nullptr;
    }

    struct ResultData {
        static constexpr int MSG_SIZE = 512;
        Result value = Result::NONE;
        std::array<char, MSG_SIZE> msg{};
        void setMessage(const std::string& message) {
            LOG_IF(WARNING, message.size() > msg.size())
                << "Message size is " << message.size()
                << " bytes, which exceeds limit";
            std::strncpy(msg.data(), message.c_str(), msg.size() - 1);
        }
        [[nodiscard]] std::string getMessage() const noexcept {
            return msg.data();
        }
    }* result;
};

inline std::ostream& operator<<(std::ostream& os,
                                const PerBuildData::Variant& variant) {
    switch (variant) {
        case PerBuildData::Variant::kUser:
            return os << "User";
        case PerBuildData::Variant::kUserDebug:
            return os << "UserDebug";
        case PerBuildData::Variant::kEng:
            return os << "Eng";
    }
    return os;
}
