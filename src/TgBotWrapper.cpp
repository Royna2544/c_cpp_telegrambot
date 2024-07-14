#include <StringResManager.hpp>
#include <TgBotWrapper.hpp>
#include "InstanceClassBase.hpp"

void TgBotWrapper::commandHandler(const command_callback_t& module_callback,
                                  unsigned int authflags, MessagePtr message) {
    static const std::string myName = getBotUser()->username;
    MessageWrapperLimited wrapper(message);

    std::string text = message->text;
    if (wrapper.hasExtraText()) {
        text = text.substr(0, text.size() - wrapper.getExtraText().size());
        boost::trim(text);
    }
    auto v = StringTools::split(text, '@');
    if (v.size() == 2 && v[1] != myName) {
        return;
    }

    if (AuthContext::getInstance()->isAuthorized(message, authflags)) {
        module_callback(this, message);
    }
}

void TgBotWrapper::addCommand(const CommandModule& module, bool isReload) {
    unsigned int authflags = AuthContext::Flags::REQUIRE_USER;
    if (!module.isEnforced()) {
        authflags |= AuthContext::Flags::PERMISSIVE;
    }
    getEvents().onCommand(
        module.command,
        [this, authflags, callback = module.fn](const Message::Ptr& message) {
            commandHandler(callback, authflags, message);
        });
    if (!isReload) {
        _modules.emplace_back(module);
    }
}

void TgBotWrapper::removeCommand(const std::string& cmd) {
    getEvents().onCommand(cmd, {});
}

MessageWrapper TgBotWrapper::getMessageWrapper(const Message::Ptr& msg) {
    return MessageWrapper(msg);
}

MessageWrapperLimited TgBotWrapper::getMessageWrapperLimited(
    const Message::Ptr& msg) {
    return MessageWrapperLimited(msg);
}

bool TgBotWrapper::setBotCommands() const {
    std::vector<TgBot::BotCommand::Ptr> buffer;
    for (const auto& cmd : _modules) {
        if (!cmd.isHideDescription()) {
            auto onecommand = std::make_shared<CommandModule>(cmd);
            if (cmd.isEnforced()) {
                onecommand->description += " " + GETSTR_BRACE(OWNER);
            }
            buffer.emplace_back(onecommand);
        }
    }
    try {
        getApi().setMyCommands(buffer);
    } catch (const TgBot::TgException& e) {
        LOG(ERROR) << GETSTR_IS(ERROR_UPDATING_BOT_COMMANDS) << e.what();
        return false;
    }
    return true;
}

std::string TgBotWrapper::getCommandModulesStr() const {
    std::stringstream ss;

    for (const auto& module : _modules) {
        ss << module.command << " ";
    }
    return ss.str();
}

bool TgBotWrapper::unloadCommand(const std::string& command) {
    if (!isKnownCommand(command)) {
        LOG(INFO) << "Command " << command << " is not present";
        return false;
    }
    auto it = findModulePosition(command);
    if (it->isLoaded) {
        removeCommand(command);
        it->isLoaded = false;
        return true;
    }
    LOG(INFO) << "Command " << command << " is already unloaded";
    return false;
}

bool TgBotWrapper::reloadCommand(const std::string& command) {
    if (!isKnownCommand(command)) {
        LOG(INFO) << "Command " << command << " is not present";
        return false;
    }
    auto it = findModulePosition(command);
    if (!it->isLoaded) {
        it->isLoaded = true;
        addCommand(*it, true);
        return true;
    }
    LOG(INFO) << "Command " << command << " is already loaded";
    return false;
}

bool TgBotWrapper::isLoadedCommand(const std::string& command) {
    if (!isKnownCommand(command)) {
        LOG(INFO) << "Command " << command << " is not present";
        return false;
    }
    return findModulePosition(command)->isLoaded;
}

bool TgBotWrapper::isKnownCommand(const std::string& command) {
    return findModulePosition(command) != _modules.end();
}

decltype(TgBotWrapper::_modules)::iterator
TgBotWrapper::findModulePosition(const std::string& command) {
    return std::ranges::find_if(
        _modules,
        [&command](const CommandModule& e) { return e.command == command; });
}

DECLARE_CLASS_INST(TgBotWrapper);