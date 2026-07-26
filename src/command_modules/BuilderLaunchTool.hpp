#pragma once

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <algorithm>
#include <api/TgBotApi.hpp>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "llm/LLMBackend.hpp"

namespace llm::builder_launch {

struct PendingBuilds {
    bool kernel = false;
    bool rom = false;
};

inline const Tool kKernelBuildTool{
    "kernelbuild",
    "Create or update a Linux kernel build plan from the admin's natural "
    "language request. Call this tool whenever the admin expresses an intent "
    "to build a kernel, even when some fields are missing; omit unknown "
    "fields and the tool will retain supplied values and report what to ask "
    "for next. A complete plan opens one final Telegram review/confirmation "
    "and never starts the build silently. Available kernels: Eureka Kernel, "
    "Grand Kernel, GrassKernel-tb128fu. Device must be compatible with the "
    "selected kernel. Optional fragment changes override the configuration "
    "defaults.",
    nlohmann::json{
        {"type", "object"},
        {"properties",
         {{"kernel",
           {{"type", "string"},
            {"description", "Kernel configuration name."},
            {"enum",
             {"Eureka Kernel", "Grand Kernel", "GrassKernel-tb128fu"}}}},
          {"device",
           {{"type", "string"},
            {"description",
             "Target device codename, such as a30 or tb128fu."}}},
          {"enable_fragments",
           {{"type", "array"},
            {"description",
             "Configuration fragment names to explicitly enable."},
            {"items", {{"type", "string"}}},
            {"uniqueItems", true}}},
          {"disable_fragments",
           {{"type", "array"},
            {"description",
             "Configuration fragment names to explicitly disable."},
            {"items", {{"type", "string"}}},
            {"uniqueItems", true}}},
          {"clone_depth",
           {{"type", "integer"},
            {"minimum", 0},
            {"description",
             "Git clone depth. Zero means a full clone; default is 1."}}},
          {"fail_on_fetch",
           {{"type", "boolean"},
            {"description",
             "Abort if an existing checkout cannot be fetched/fast-forwarded. "
             "Defaults to false."}}},
          {"reset",
           {{"type", "boolean"},
            {"description",
             "Clear the pending kernel plan before applying these fields."}}}}},
        {"additionalProperties", false}}};

inline const Tool kRomBuildTool{
    "rombuild",
    "Create or update an Android ROM/recovery build plan from the admin's "
    "natural language request. Call this tool whenever the admin expresses "
    "an intent to build Android, even when fields are missing; omit unknown "
    "fields and the tool will retain supplied values and report what to ask "
    "for next. A complete plan opens one final Telegram review/confirmation "
    "and never starts the build silently. Core fields are device, ROM, "
    "Android version, and variant. Available ROMs include Evolution-X, "
    "DerpFestNew, DerpFest, LineageOS, crDroid, Yaap, and TWRP.",
    nlohmann::json{
        {"type", "object"},
        {"properties",
         {{"device",
           {{"type", "string"},
            {"description",
             "Target device codename, such as a30 or tb128fu."}}},
          {"rom",
           {{"type", "string"},
            {"description", "ROM or recovery name."},
            {"enum",
             {"Evolution-X", "DerpFestNew", "DerpFest", "LineageOS", "crDroid",
              "Yaap", "TWRP"}}}},
          {"android_version",
           {{"type", "number"},
            {"description", "Android platform version."},
            {"enum", {11, 12, 12.1, 13, 14, 15, 16}}}},
          {"variant",
           {{"type", "string"},
            {"description", "Android build variant."},
            {"enum", {"user", "userdebug", "eng"}}}},
          {"manifest",
           {{"type", "string"},
            {"description",
             "Optional local-manifest name used to disambiguate compatible "
             "configurations."}}},
          {"repo_sync",
           {{"type", "boolean"},
            {"description", "Sync sources before building; default true."}}},
          {"clean_build",
           {{"type", "boolean"},
            {"description",
             "Request a clean build; default is server policy."}}},
          {"use_ccache",
           {{"type", "boolean"},
            {"description", "Use ccache; default is server policy."}}},
          {"use_rbe",
           {{"type", "boolean"},
            {"description",
             "Use configured remote build execution. Only set when explicitly "
             "requested."}}},
          {"upload",
           {{"type", "string"},
            {"description", "Artifact delivery method; default is none."},
            {"enum", {"none", "local", "stream", "gofile"}}}},
          {"parallel_jobs",
           {{"type", "integer"},
            {"minimum", 1},
            {"description",
             "Parallel build jobs; omit to use the server default."}}},
          {"force_checkout",
           {{"type", "boolean"},
            {"description",
             "Force-overwrite local source changes. This is destructive; set "
             "true only when explicitly requested by the admin."}}},
          {"reset",
           {{"type", "boolean"},
            {"description",
             "Clear the pending ROM plan before applying these fields."}}}}},
        {"additionalProperties", false}}};

namespace detail {

struct PendingPlans {
    nlohmann::json kernel = nlohmann::json::object();
    nlohmann::json rom = nlohmann::json::object();
};

inline auto registry() {
    static std::mutex mutex;
    static std::unordered_map<std::string, PendingPlans> plans;
    return std::pair{&mutex, &plans};
}

inline std::string keyFor(const Message::Ptr& message) {
    if (!message || !message->chat || !message->from || !*message->from) {
        return {};
    }
    return fmt::format("{}:{}", message->chat->id, (*message->from)->id);
}

inline std::vector<std::string_view> requiredFields(std::string_view name) {
    if (name == kKernelBuildTool.name) {
        return {"kernel", "device"};
    }
    return {"device", "rom", "android_version", "variant"};
}

inline std::vector<std::string> missingFields(std::string_view name,
                                              const nlohmann::json& plan) {
    std::vector<std::string> missing;
    for (const auto field : requiredFields(name)) {
        const auto it = plan.find(field);
        if (it == plan.end() || it->is_null() ||
            (it->is_string() && it->get_ref<const std::string&>().empty())) {
            missing.emplace_back(field);
        }
    }
    return missing;
}

inline bool validateCommon(std::string_view name, const nlohmann::json& plan,
                           std::string* error) {
    const auto requireString = [&](std::string_view field) {
        const auto it = plan.find(field);
        if (it != plan.end() && !it->is_string()) {
            *error = fmt::format("{} must be a string.", field);
            return false;
        }
        return true;
    };
    if (name == kKernelBuildTool.name) {
        if (!requireString("kernel") || !requireString("device")) {
            return false;
        }
        for (const auto field : {"enable_fragments", "disable_fragments"}) {
            const auto it = plan.find(field);
            if (it != plan.end() &&
                (!it->is_array() ||
                 !std::ranges::all_of(*it, [](const auto& value) {
                     return value.is_string();
                 }))) {
                *error = fmt::format("{} must be an array of strings.", field);
                return false;
            }
        }
        if (const auto it = plan.find("clone_depth");
            it != plan.end() &&
            (!it->is_number_integer() || it->get<int>() < 0)) {
            *error = "clone_depth must be a non-negative integer.";
            return false;
        }
        return true;
    }

    if (!requireString("device") || !requireString("rom") ||
        !requireString("variant") || !requireString("manifest")) {
        return false;
    }
    if (const auto it = plan.find("android_version");
        it != plan.end() && !it->is_number()) {
        *error = "android_version must be numeric.";
        return false;
    }
    if (const auto it = plan.find("parallel_jobs");
        it != plan.end() && (!it->is_number_integer() || it->get<int>() < 1)) {
        *error = "parallel_jobs must be a positive integer.";
        return false;
    }
    return true;
}

}  // namespace detail

inline PendingBuilds pendingBuilds(ChatId chatId, UserId userId) {
    const auto key = fmt::format("{}:{}", chatId, userId);
    auto [mutex, plans] = detail::registry();
    std::lock_guard lock(*mutex);
    const auto it = plans->find(key);
    if (it == plans->end()) {
        return {};
    }
    return {
        .kernel = !it->second.kernel.empty(),
        .rom = !it->second.rom.empty(),
    };
}

inline std::string pendingContext(ChatId chatId, UserId userId) {
    const auto key = fmt::format("{}:{}", chatId, userId);
    auto [mutex, plans] = detail::registry();
    std::lock_guard lock(*mutex);
    const auto it = plans->find(key);
    if (it == plans->end() ||
        (it->second.kernel.empty() && it->second.rom.empty())) {
        return {};
    }
    return fmt::format(
        "\n\nPending builder plans for this admin: kernel={}, ROM={}. "
        "Treat short follow-up replies as values for the missing fields and "
        "call the corresponding builder tool again.",
        it->second.kernel.dump(), it->second.rom.dump());
}

inline ToolExecutor makeExecutor(TgBotApi::Ptr api, Message::Ptr message) {
    return [api, message = std::move(message)](const std::string& name,
                                               const nlohmann::json& input,
                                               bool& isError) -> std::string {
        isError = false;
        if (name != kKernelBuildTool.name && name != kRomBuildTool.name) {
            isError = true;
            return fmt::format("Unknown builder tool: {}", name);
        }
        if (!input.is_object()) {
            isError = true;
            return fmt::format("{} expects a JSON object.", name);
        }

        const auto key = detail::keyFor(message);
        if (key.empty()) {
            isError = true;
            return "Builder tools require an authenticated Telegram user/chat.";
        }

        nlohmann::json plan;
        {
            auto [mutex, plans] = detail::registry();
            std::lock_guard lock(*mutex);
            auto& pending = (*plans)[key];
            auto& target =
                name == kKernelBuildTool.name ? pending.kernel : pending.rom;
            if (input.value("reset", false)) {
                target = nlohmann::json::object();
            }
            for (auto it = input.begin(); it != input.end(); ++it) {
                if (it.key() != "reset") {
                    target[it.key()] = it.value();
                }
            }
            plan = target;
        }

        std::string validationError;
        if (!detail::validateCommon(name, plan, &validationError)) {
            isError = true;
            return validationError;
        }

        const auto missing = detail::missingFields(name, plan);
        if (!missing.empty()) {
            return fmt::format(
                "{} plan updated but is incomplete. Current plan: {}. "
                "Missing required fields: {}. Ask the admin only for those "
                "fields, then call {} again with the answer.",
                name, plan.dump(), fmt::join(missing, ", "), name);
        }

        plan["source"] = "llm";
        if (!api->invokeCommand(name, message, plan.dump())) {
            isError = true;
            return fmt::format(
                "Failed to stage the {} plan. The command may be unavailable, "
                "unauthorized, or busy.",
                name);
        }

        {
            auto [mutex, plans] = detail::registry();
            std::lock_guard lock(*mutex);
            auto it = plans->find(key);
            if (it != plans->end()) {
                if (name == kKernelBuildTool.name) {
                    it->second.kernel = nlohmann::json::object();
                } else {
                    it->second.rom = nlohmann::json::object();
                }
                if (it->second.kernel.empty() && it->second.rom.empty()) {
                    plans->erase(it);
                }
            }
        }

        return fmt::format(
            "Staged the complete {} plan in Telegram: {}. The admin must "
            "review and confirm the final request there before execution.",
            name, plan.dump());
    };
}

}  // namespace llm::builder_launch
