#include <api/components/ModuleManagement.hpp>
#include <libfs.hpp>

#include "api/CommandModule.hpp"
#include "api/TgBotApiImpl.hpp"

CommandModule* TgBotApiImpl::ModulesManagement::operator[](
    const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex);
    if (_handles.contains(name)) {
        return _handles.at(name).get();
    }
    return nullptr;
}

bool TgBotApiImpl::ModulesManagement::operator+=(CommandModule::Ptr module) {
    std::string moduleName;

    // Load the library (dlopen)
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (module == nullptr) {
            LOG(WARNING) << "Invalid module";
            return false;
        }
        if (!module->load()) {
            LOG(ERROR) << "Failed to load module";
            return false;
        }
        moduleName = module->_module->name;
        if (_handles.contains(moduleName)) {
            LOG(WARNING) << fmt::format("Module with name {} already loaded. REJECT", moduleName);
            return false;
        }
        _handles.emplace(moduleName, std::move(module));
    }
    // Register the command
    (*this) += moduleName;
    return true;
}

bool TgBotApiImpl::ModulesManagement::operator+=(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!_handles.contains(name)) {
        LOG(WARNING) << "Module with name " << name
                     << " doesn't exist to load.";
        return false;
    }
    if (!_handles.at(name)->isLoaded()) {
        if (!_handles.at(name)->load()) {
            LOG(ERROR) << "Failed to load module with name " << name;
            return false;
        }
    }

    auto authflags = AuthContext::Flags::REQUIRE_USER;
    if (!_handles.at(name)->_module->isEnforced()) {
        authflags |= AuthContext::Flags::PERMISSIVE;
    }

    _api->getEvents().onCommand(name, [this, authflags,
                                       cmd = name](Message::Ptr message) {
        commandAsync.emplaceTask(
            cmd, std::async(std::launch::async, &TgBotApiImpl::commandHandler,
                            _api, cmd, authflags, std::move(message)));
    });
    return true;
}

bool TgBotApiImpl::ModulesManagement::operator-=(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!_handles.contains(name)) {
        LOG(WARNING) << "Module with name " << name
                     << " doesn't exist to load.";
        return false;
    }
    if (_handles.at(name)->isLoaded()) {
        if (!_handles.at(name)->unload()) {
            LOG(ERROR) << "Failed to unload module with name " << name;
            return false;
        }
    }
    // Erase command handler
    _api->getEvents().onCommand(name, {});
    return true;
}

bool TgBotApiImpl::ModulesManagement::loadFrom(
    const std::filesystem::path& directory) {
    std::error_code ec;

    LOG(INFO) << "Loading commands from " << directory;
    for (const auto& it : std::filesystem::directory_iterator(directory, ec)) {
        const auto filename = it.path().filename();
        if (filename.string().starts_with(CommandModule::prefix) &&
            filename.extension() == FS::kDylibExtension) {
            (*this) += std::make_unique<CommandModule>(it);
        }
    }
    if (ec) {
        LOG(ERROR) << "Failed to iterate through modules: " << ec.message();
        return false;
    }
    std::vector<TgBot::BotCommand::Ptr> buffer;
    buffer.reserve(_handles.size());
    for (const auto& [name, mod] : _handles) {
        if (!mod->_module->isHideDescription()) {
            auto onecommand = std::make_shared<TgBot::BotCommand>();
            onecommand->command = mod->_module->name;
            onecommand->description = mod->_module->description;
            if (mod->_module->isEnforced()) {
                onecommand->description += " (Owner)";
            }
            buffer.emplace_back(onecommand);
        }
    }
    try {
        _api->getApi().setMyCommands(buffer);
    } catch (const TgBot::TgException& e) {
        LOG(ERROR) << fmt::format("Error updating bot commands list: {}",
                                  e.what());
        return false;
    }
    return true;
}

TgBotApiImpl::ModulesManagement::ModulesManagement(
    TgBotApiImpl::Ptr api, const std::filesystem::path& modules_dir)
    : _api(api), commandAsync(2) {
    loadFrom(modules_dir);
}
