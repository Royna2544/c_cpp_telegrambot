#pragma once

#include <TgBotUtilsExports.h>
#include <trivial_helpers/fruit_inject.hpp>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

/**
 * @brief Interface for providing resources.
 *
 * This interface defines the methods for getting and preloading resources.
 */
struct ResourceProvider {
    /**
     * @brief Get a resource by its filename.
     *
     * This method retrieves the content of a resource specified by its filename.
     *
     * @param filename The path to the resource file.
     * @return A string_view containing the content of the resource.
     */
    virtual std::string_view get(std::filesystem::path filename) const = 0;

    /**
     * @brief Preload a resource.
     *
     * This method preloads a resource specified by its path.
     *
     * @param p The path to the resource file.
     * @return True if the resource was successfully preloaded, false otherwise.
     */
    virtual bool preload(std::filesystem::path p) = 0;

    virtual ~ResourceProvider() = default;
};

struct TgBotUtils_API ResourceManager : ResourceProvider {
    [[nodiscard]] std::string_view get(std::filesystem::path filename) const override;
    bool preload(std::filesystem::path p) override;

    explicit ResourceManager(std::filesystem::path resourceDirectory);
    ~ResourceManager() override = default;

   private:
    std::unordered_map<std::filesystem::path, std::string> kResources;
    std::vector<std::string> ignoredResources;
    std::filesystem::path m_resourceDirectory;
    static constexpr std::string_view kResourceLoadIgnoreFile = ".loadignore";
};
