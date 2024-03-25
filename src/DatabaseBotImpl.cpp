#include <BotReplyMessage.h>
#include <Database.h>
#include <SingleThreadCtrl.h>
#include "InstanceClassBase.hpp"

DECLARE_CLASS_INST(database::DatabaseWrapperBotImplObj);

namespace database {

struct DatabaseSync : SingleThreadCtrlRunnable<> {
    void runFunction() override {
        while (kRun) {
            DatabaseWrapperBotImplObj::getInstance().save();
            delayUnlessStop(100);
        }
    }
};

void DatabaseWrapperBotImpl::load() {
    auto& mgr = SingleThreadCtrlManager::getInstance();
    DatabaseWrapperImpl::load();
    const SingleThreadCtrlManager::GetControllerRequest req{
        .usage = SingleThreadCtrlManager::USAGE_DATABASE_SYNC_THREAD,
        .flags = SingleThreadCtrlManager::GetControllerFlags::REQUIRE_NONEXIST |
                 SingleThreadCtrlManager::GetControllerFlags::
                     REQUIRE_FAILACTION_RETURN_NULL};
    if (const auto it = mgr.getController<DatabaseSync>(req); it) it->run();

    whitelist = std::make_shared<ProtoDatabase>(
        _bot, "whitelist", protodb.mutable_whitelist()->mutable_id());
    blacklist = std::make_shared<ProtoDatabase>(
        _bot, "blacklist", protodb.mutable_blacklist()->mutable_id());
    whitelist->setOtherProtoDatabaseBase(blacklist);
    blacklist->setOtherProtoDatabaseBase(whitelist);
}

bool ProtoDatabase::rejectUid(const User::Ptr& user) const {
    if (_bot.getApi().getMe()->id == user->id) return true;
    if (DatabaseWrapperImplObj::getInstance().maybeGetOwnerId() == user->id) return true;
    if (user->isBot) return true;
    return false;
}

void ProtoDatabase::onAlreadyExist(const Message::Ptr& message,
                                   const User::Ptr& who,
                                   const ProtoDatabaseBase* which) const {
    std::string text = appendListName("already in", who, which);
    if (which != this) {
        text += " Remove the user from ";
        text += which->name;
        text += " first.";
    }
    bot_sendReplyMessage(_bot, message, text);
}
void ProtoDatabase::onAdded(const Message::Ptr& message, const User::Ptr& who,
                            const ProtoDatabaseBase* which) const {
    bot_sendReplyMessage(_bot, message, appendListName("added to", who, which));
}
void ProtoDatabase::onNotFound(const Message::Ptr& message,
                               const User::Ptr& who,
                               const ProtoDatabaseBase* which) const {
    bot_sendReplyMessage(_bot, message,
                         appendListName("not found in", who, which));
}
void ProtoDatabase::onRemoved(const Message::Ptr& message, const User::Ptr& who,
                              const ProtoDatabaseBase* which) const {
    bot_sendReplyMessage(_bot, message,
                         appendListName("removed from", who, which));
}
void ProtoDatabase::onUserNotFoundOnMessage(const Message::Ptr& message) const {
    bot_sendReplyMessage(_bot, message, "Reply to a user.");
}
std::string ProtoDatabase::appendListName(const std::string& op,
                                          const User::Ptr from,
                                          const ProtoDatabaseBase* what) {
    std::stringstream ss;
    ss << "User " << from->id << ' ' << op << ' ' << what->name << '.';
    return ss.str();
}

}  // namespace database
