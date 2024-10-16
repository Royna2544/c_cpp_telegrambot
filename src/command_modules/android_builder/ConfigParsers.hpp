#pragma once

#include <absl/log/log.h>

#include <memory>
#include <ostream>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

#include "RepoUtils.hpp"

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
        bool operator()(std::string_view filename, const bool debug = false) const {
            return matcher_(filename, data_, debug);
        }
        bool operator==(const ArtifactMatcher &other) const {
            return name_ == other.name_;
        }
        void setData(std::string data) { data_ = std::move(data); }
    };

    struct ROMInfo {
        using Ptr = std::shared_ptr<ROMInfo>;

        std::string name;               // name of the ROM
        std::string url;                // URL of the ROM repo
        std::string target;             // build target to build a ROM
        ArtifactMatcher::Ptr artifact;  // matcher for the out artifact

        bool operator==(const ROMInfo &other) const = default;
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
            return romInfo->name + " (Android " +
                   std::to_string(androidVersion) + ")";
        }

        bool operator==(const ROMBranch &other) const {
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

        bool operator==(const Device &other) const {
            return codename == other.codename;
        }
    };

    struct LocalManifest {
        struct PrepareBase {
            virtual ~PrepareBase() = default;
            // Do the preparation
            // With the manifest path being `path'
            virtual bool operator()(const std::filesystem::path &path) = 0;
        };

        struct GitPrepare : PrepareBase {
            RepoUtils::RepoInfo info;
            explicit GitPrepare(RepoUtils::RepoInfo info)
                : info(std::move(info)) {}
            bool operator()(const std::filesystem::path &path) override;
        };

        struct WritePrepare : PrepareBase {
            struct Data : RepoInfo {
                std::filesystem::path destination;

                explicit Data(std::string url, std::string branch,
                              std::filesystem::path destination)
                    : RepoInfo(std::move(url), std::move(branch)),
                      destination(std::move(destination)) {}
            };
            std::vector<Data> data;
            bool operator()(const std::filesystem::path &path) override;
        };

        using Ptr = std::shared_ptr<LocalManifest>;
        template <typename match>
        using StringArrayOr =
            std::variant<std::vector<std::string>, std::vector<match>>;

        struct ROMLookup {
            int androidVersion;  // android version of the ROM
            std::string name;    // name of the ROM
        };

        std::string name;  // name of the manifest
        // associated ROM and its branch
        std::variant<ROMLookup, ROMBranch::Ptr> rom;
        std::shared_ptr<PrepareBase> prepare;  // local manifest information
        // First type is used before merge, second type is used after merge
        StringArrayOr<Device::Ptr> devices;
        long job_count;  // number of jobs
    };

    explicit ConfigParser(const std::filesystem::path &jsonFileDir);

    // Get available devices from config
    [[nodiscard]] std::set<Device::Ptr> getDevices() const;
    // Get available ROM branches for a given device
    [[nodiscard]] std::set<ROMBranch::Ptr> getROMBranches(
        const Device::Ptr &device) const;
    // Get a local manifest for a given ROM branch and device
    [[nodiscard]] LocalManifest::Ptr getLocalManifest(
        const ROMBranch::Ptr &romBranch, const Device::Ptr &device) const;

    /**
     * @brief Finds a device by its codename in the configuration.
     *
     * This function iterates through the available devices and checks if the
     * codename matches the given parameter. If a match is found, a shared
     * pointer to the device is returned. If no match is found, a nullptr is
     * returned.
     *
     * @param codename The codename of the device to search for.
     * @return A shared pointer to the device if found, or nullptr if not found.
     */
    [[nodiscard]] Device::Ptr getDevice(const std::string_view &codename) const;

    /**
     * @brief Finds a ROM branch by its branch name in the configuration.
     *
     * This function iterates through the available ROM branches and checks if
     * the branch name matches the given parameter. If a match is found, a
     * shared pointer to the ROM branch is returned. If no match is found, a
     * nullptr is returned.
     *
     * @param romName The string representitive of the ROM name to search for
     * @param androidVersion The android version of the ROM branch to search for
     * @return A shared pointer to the ROM branch if found, or nullptr if not
     * found.
     */
    [[nodiscard]] ROMBranch::Ptr getROMBranches(const std::string &romName,
                                                const int androidVersion) const;

   private:
    std::vector<LocalManifest::Ptr> parsedManifests;

    class Parser;
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

// Helpers for std variant codebloat, as we have real data on index 1
static constexpr int VALUE_INDEX = 1;

template <typename Variant>
[[nodiscard]] auto getValue(Variant &&variant) {
    return std::get<VALUE_INDEX>(std::forward<Variant>(variant));
}

inline std::ostream &operator<<(std::ostream &os,
                                const PerBuildData::Variant &variant) {
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