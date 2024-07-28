#include <ConfigParsers.hpp>
#include <TgBotWrapper.hpp>
#include <filesystem>
#include <future>
#include <initializer_list>
#include <libos/libfs.hpp>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <tasks/ROMBuildTask.hpp>
#include <tasks/RepoSyncTask.hpp>
#include <tasks/UploadFileTask.hpp>

#include "PythonClass.hpp"
#include "absl/log/check.h"
#include "absl/strings/match.h"
#include "absl/strings/str_split.h"
#include "absl/strings/strip.h"
#include "tgbot/types/InlineKeyboardButton.h"
#include "tgbot/types/InlineKeyboardMarkup.h"

namespace {

std::string getSystemInfo() {
    auto py = PythonClass::get();
    auto mod = py->importModule("system_info");
    if (!mod) {
        LOG(ERROR) << "Failed to import system_info module";
        return "";
    }
    auto fn = mod->lookupFunction("get_system_summary");
    if (!fn) {
        LOG(ERROR) << "Failed to find get_system_summary function";
        return "";
    }
    std::string summary;
    if (!fn->call(nullptr, &summary)) {
        LOG(ERROR) << "Failed to call get_system_summary function";
        return "";
    }
    return summary;
}

bool repoSync(PerBuildData data, ApiPtr wrapper, MessagePtr message) {
    PerBuildData::ResultData result{};
    data.result = &result;
    RepoSyncTask repoSync(data);

    // Send a message to notify about the start of the build process
    auto reposyncMsg = wrapper->sendReplyMessage(message, "Now syncing...");

    do {
        bool execRes = repoSync.execute();
        if (!execRes) {
            LOG(ERROR) << "Failed to exec";
            wrapper->editMessage(reposyncMsg, "Failed to execute process");
            return false;
        }
    } while (result.value == PerBuildData::Result::ERROR_NONFATAL);

    wrapper->editMessage(reposyncMsg, result.getMessage());
    switch (result.value) {
        case PerBuildData::Result::SUCCESS:
            return true;
        case PerBuildData::Result::ERROR_FATAL:
            LOG(ERROR) << "Repo sync failed";
            break;
        default:
            break;
    }
    return false;
}

namespace fs = std::filesystem;

bool build(PerBuildData data, ApiPtr wrapper, MessagePtr message) {
    std::error_code ec;
    PerBuildData::ResultData result{};

    data.result = &result;
    auto msg = wrapper->sendReplyMessage(message, "Starting build...");

    ROMBuildTask romBuild(wrapper, msg, data);

    do {
        bool execRes = romBuild.execute();
        if (!execRes) {
            LOG(ERROR) << "Failed to exec";
            wrapper->editMessage(msg,
                                 "Build failed: Couldn't start build process");
            return false;
        }
    } while (result.value == PerBuildData::Result::ERROR_NONFATAL);

    switch (result.value) {
        case PerBuildData::Result::ERROR_FATAL:
            LOG(ERROR) << "Failed to build ROM";
            wrapper->editMessage(msg, "Build failed:\n" + result.getMessage());
            if (fs::file_size(ROMBuildTask::kErrorLogFile, ec) != 0U) {
                if (ec) {
                    break;
                }
                wrapper->sendDocument(
                    message->chat->id,
                    TgBot::InputFile::fromFile(
                        ROMBuildTask::kErrorLogFile.data(), "text/plain"));
            }
            break;
        case PerBuildData::Result::SUCCESS:
            wrapper->editMessage(msg, "Build completed successfully");
            return true;
        case PerBuildData::Result::ERROR_NONFATAL:
            break;
    }
    return false;
}

void upload(PerBuildData data, ApiPtr wrapper, MessagePtr message) {
    PerBuildData::ResultData uploadResult;
    auto uploadmsg = wrapper->sendMessage(message, "Will upload");

    data.result = &uploadResult;
    UploadFileTask uploadTask(data);
    if (!uploadTask.execute()) {
        wrapper->editMessage(uploadmsg, "Could'nt initialize upload");
    } else {
        wrapper->editMessage(uploadmsg, uploadResult.getMessage());
        if (uploadResult.value == PerBuildData::Result::ERROR_FATAL) {
            wrapper->sendDocument(
                message->chat->id,
                TgBot::InputFile::fromFile("upload_err.txt", "text/plain"));
        }
    }
}
}  // namespace

template <int Y>
TgBot::InlineKeyboardMarkup::Ptr buildKeyboard(
    std::initializer_list<std::pair<std::string, std::string>> list) {
    std::vector<std::vector<TgBot::InlineKeyboardButton::Ptr>> keyboardButtons(
        1);
    int x = 0;
    for (const auto& [text, callbackData] : list) {
        auto button = std::make_shared<TgBot::InlineKeyboardButton>();
        button->text = text;
        button->callbackData = callbackData;
        keyboardButtons.back().emplace_back(button);
        ++x;
        if (keyboardButtons.back().size() == Y && x != list.size()) {
            keyboardButtons.emplace_back();
        }
    }
    const auto keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
    keyboard->inlineKeyboard = keyboardButtons;
    return keyboard;
}

DECLARE_COMMAND_HANDLER(rombuild, tgWrapper, message) {
    MessageWrapper wrapper(tgWrapper, message);
    constexpr static std::string_view kBuildDirectory = "rom_build/";
    const auto cwd = std::filesystem::current_path();
    const auto returnToCwd = [cwd]() {
        std::error_code _ec;
        std::filesystem::current_path(cwd, _ec);
        if (_ec) {
            LOG(ERROR) << "Failed to restore cwd: " << _ec.message();
        }
        PythonClass::deinit();
    };

    PythonClass::init();

    static auto buildRoot =
        FS::getPathForType(FS::PathType::GIT_ROOT) / "src" / "android_builder";

    PythonClass::get()->addLookupDirectory(buildRoot / "scripts");

    auto keyboard = std::make_shared<TgBot::InlineKeyboardMarkup>();
    keyboard = buildKeyboard<2>({{"Build ROM", "build"},
                                 {"Send system info", "send_system_info"},
                                 {"Settings", "settings"},
                                 {"Close", "close"}});

    auto m =
        tgWrapper->sendReplyMessage(message, "Will build ROM...", keyboard);

    ConfigParser parser(buildRoot / "configs" / "builder_config.xml");

    tgWrapper->onCallbackQuery([=](const TgBot::CallbackQuery::Ptr& query) {
        struct QueryData {
            bool do_repo_sync = true;
            PerBuildData per_build;
            MessageId messageId;
            std::vector<ConfigParser::ROMEntry> roms;
        };

        static std::optional<QueryData> queryData;

        if (queryData && queryData->messageId != m->messageId) {
            return;
        }
        if (!queryData) {
            queryData.emplace();
            queryData->messageId = m->messageId;
        }

        if (query->data == "back") {
            tgWrapper->editMessage(m, "Will build ROM...");
            tgWrapper->editMessageMarkup(m, keyboard);
            queryData->per_build.device.clear();
            queryData->per_build.localManifest.reset();
        } else if (query->data == "build") {
            tgWrapper->editMessage(m, "Select device...");
            auto deviceKeyboard =
                std::make_shared<TgBot::InlineKeyboardMarkup>();
            for (const auto& device : parser.getDevices()) {
                auto button = std::make_shared<TgBot::InlineKeyboardButton>();
                button->text = device.device->toString();
                button->callbackData = "device_" + device.device->codename;
                deviceKeyboard->inlineKeyboard.emplace_back();
                deviceKeyboard->inlineKeyboard.back().emplace_back(button);
            }
            deviceKeyboard->inlineKeyboard.push_back(
                {std::make_shared<TgBot::InlineKeyboardButton>("Back", "",
                                                               "back")});
            tgWrapper->editMessageMarkup(m, deviceKeyboard);
        } else if (query->data == "settings") {
            auto settingKeyboard = buildKeyboard<2>(
                {{"Run repo sync", "repo_sync"}, {"Back", "back"}});
            tgWrapper->editMessage(m, "Settings...");
            tgWrapper->editMessageMarkup(m, settingKeyboard);
        } else if (query->data == "send_system_info") {
            static std::string info = getSystemInfo();
            auto backKeyboard = buildKeyboard<2>({{"Back", "back"}});
            tgWrapper->editMessage(m, info);
            tgWrapper->editMessageMarkup(m, backKeyboard);
        } else if (query->data == "repo_sync") {
            queryData->do_repo_sync = !queryData->do_repo_sync;
            tgWrapper->answerCallbackQuery(
                query->id, std::string() + "Repo sync enabled: " +
                               (queryData->do_repo_sync ? "Yes" : "No"));
        } else if (query->data == "close") {
            tgWrapper->editMessage(m, "Closed");
            tgWrapper->editMessageMarkup(m, nullptr);
            queryData.reset();
        } else if (absl::StartsWith(query->data, "device_")) {
            std::string_view device = query->data;
            absl::ConsumePrefix(&device, "device_");
            auto it = parser.getDevices();
            queryData->roms =
                std::ranges::find_if(it, [&](const auto& d) {
                    return d.device->codename == std::string(device);
                })->getROMs();
            queryData->per_build.device = device;
            auto deviceKeyboard =
                std::make_shared<TgBot::InlineKeyboardMarkup>();
            for (const auto& device : queryData->roms) {
                auto button = std::make_shared<TgBot::InlineKeyboardButton>();
                button->text = device.romName + " Android " +
                               std::to_string(device.androidVersion);
                button->callbackData = "rom_" + device.romName + "_" +
                                       std::to_string(device.androidVersion);
                deviceKeyboard->inlineKeyboard.emplace_back();
                deviceKeyboard->inlineKeyboard.back().emplace_back(button);
            }
            deviceKeyboard->inlineKeyboard.push_back(
                {std::make_shared<TgBot::InlineKeyboardButton>("Back", "",
                                                               "back")});
            tgWrapper->editMessage(m, "Select ROM...");
            tgWrapper->editMessageMarkup(m, deviceKeyboard);
        } else if (absl::StartsWith(query->data, "rom_")) {
            std::string_view rom = query->data;
            absl::ConsumePrefix(&rom, "rom_");
            std::vector<std::string> c = absl::StrSplit(rom, '_');
            CHECK(c.size() == 2);
            auto romName = c[0];
            // Basically an assert
            int androidVersion = std::stoi(c[1]);
            auto it = std::ranges::find_if(
                queryData->roms, [=](ConfigParser::ROMEntry it) {
                    return it.androidVersion == androidVersion &&
                           it.romName == romName;
                });
            queryData->per_build.localManifest = it->getLocalManifest();

            auto variantKeyboard =
                buildKeyboard<2>({{"User build", "type_user"},
                                  {"Userdebug build", "type_userdebug"},
                                  {"Eng build", "type_eng"}});
            tgWrapper->editMessage(m, "Select build variant...");
            tgWrapper->editMessageMarkup(m, variantKeyboard);
        } else if (absl::StartsWith(query->data, "type_")) {
            std::string_view type = query->data;
            absl::ConsumePrefix(&type, "type_");
            if (type == "user") {
                queryData->per_build.variant = PerBuildData::Variant::kUser;
            } else if (type == "userdebug") {
                queryData->per_build.variant =
                    PerBuildData::Variant::kUserDebug;
            } else if (type == "eng") {
                queryData->per_build.variant = PerBuildData::Variant::kEng;
            }
            std::stringstream confirm;

            confirm << "Build variant: " << type << "\n";
            confirm << "Device: " << queryData->per_build.device << "\n";
            confirm << "Rom: "
                    << queryData->per_build.localManifest->rom->romInfo->name
                    << "\n";
            confirm << "Android version: "
                    << queryData->per_build.localManifest->rom->androidVersion
                    << "\n";

            auto keyboardConfirm =
                buildKeyboard<2>({{"Confirm", "confirm"}, {"Back", "back"}});
            tgWrapper->editMessage(m, confirm.str());
            tgWrapper->editMessageMarkup(m, keyboardConfirm);
        } else if (query->data == "confirm") {
            tgWrapper->editMessage(m, "Building...");
            std::error_code ec;
            std::filesystem::create_directory(kBuildDirectory, ec);
            if (ec && ec != std::make_error_code(std::errc::file_exists)) {
                LOG(ERROR) << "Failed to create build directory: "
                           << ec.message();
                tgWrapper->sendMessage(m, "Failed to create build directory");
                returnToCwd();
                return;
            }
            std::filesystem::current_path(kBuildDirectory, ec);
            if (queryData->do_repo_sync) {
                if (!repoSync(queryData->per_build, tgWrapper, m)) {
                    tgWrapper->editMessage(m, "Failed to sync repo");
                    returnToCwd();
                    return;
                }
            }
            if (build(queryData->per_build, tgWrapper, m)) {
                upload(queryData->per_build, tgWrapper, m);
            }
            tgWrapper->editMessage(m, "Build complete");
            returnToCwd();
        }
    });
}

DYN_COMMAND_FN(n, module) {
    module.command = "rombuild";
    module.description = "Build a ROM, I'm lazy";
    module.flags = CommandModule::Flags::Enforced;
    module.fn = COMMAND_HANDLER_NAME(rombuild);
    return true;
}
