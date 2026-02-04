#pragma once

#include <absl/log/log.h>
#include <fmt/format.h>

#include <filesystem>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

struct RepoInfo {
    std::string url;
    std::string branch;
};

class ConfigParser {
   public:
    struct ArtifactMatcher {
        using Ptr = std::shared_ptr<ArtifactMatcher>;
        using MatcherType =
            std::function<bool(const std::string_view filename,
                               const std::string_view data, const bool debug)>;
        ArtifactMatcher(MatcherType matcher, std::string name)
            : matcher_(std::move(matcher)), name_(std::move(name)) {}

       private:
        MatcherType matcher_;
        std::string name_;
        std::string data_;

       public:
        [[nodiscard]] bool match(std::string_view filename,
                                 const bool debug = false) const {
            return matcher_(filename, data_, debug);
        }
        bool operator==(const ArtifactMatcher& other) const {
            return name_ == other.name_;
        }
        void setData(std::string data) { data_ = std::move(data); }
    };

    // Artifact matcher
    struct MatcherStorage {
        std::unordered_map<std::string, ArtifactMatcher::Ptr> artifactMatchers;
        void add(const std::string& name,
                 const ArtifactMatcher::MatcherType& fn) {
            artifactMatchers[name] =
                std::make_shared<ArtifactMatcher>(fn, name);
        }
        ArtifactMatcher::Ptr get(const std::string& name) const {
            if (!artifactMatchers.contains(name)) {
                LOG(INFO) << fmt::format("No artifact matcher found for '{}'",
                                         name);
                return nullptr;
            }
            return std::make_shared<ArtifactMatcher>(
                *artifactMatchers.at(name));
        }
    };

    struct ROMInfo {
        using Ptr = std::shared_ptr<ROMInfo>;

        std::string name;               // name of the ROM
        std::string url;                // URL of the ROM repo
        std::string target;             // build target to build a ROM
        ArtifactMatcher::Ptr artifact;  // matcher for the out artifact

        bool operator==(const ROMInfo& other) const = default;
    };

    struct ROMBranch {
        using Ptr = std::shared_ptr<ROMBranch>;

        std::string branch;  // branch of the repo
        int androidVersion;  // android version of the branch

        ROMInfo::Ptr romInfo;  // associated ROMInfo

        // Returns a string representation of the ROM branch
        [[nodiscard]] std::string toString() const {
            if (!romInfo) {
                return {};
            }
            return fmt::format("{} (Android {})", romInfo->name,
                               androidVersion);
        }
        [[nodiscard]] static std::string makeKey(std::string_view name,
                                                 const int androidVersion) {
            return fmt::format("{}-{}", name, androidVersion);
        }
        [[nodiscard]] std::string makeKey() const {
            if (!romInfo) {
                throw std::logic_error("No rom info available");
            }
            return makeKey(romInfo->name, androidVersion);
        }

        bool operator==(const ROMBranch& other) const {
            return branch == other.branch &&
                   androidVersion == other.androidVersion &&
                   *romInfo == *other.romInfo;
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
    std::filesystem::path _jsonFileDir;
    MatcherStorage storage;
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
