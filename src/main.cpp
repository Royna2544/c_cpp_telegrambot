#include <Authorization.h>
#include <BotAddCommand.h>
#include <ConfigManager.h>
#include <Database.h>
#include <RegEXHandler.h>
#include <SingleThreadCtrl.h>
#include <SpamBlock.h>
#include <internal/_std_chrono_templates.h>
#include <libos/libfs.h>
#include <libos/libsighandler.h>
#include <socket/SocketInterfaceBase.h>

// Generated cmd list
#include <cmds.gen.h>

#include "CompilerInTelegram.h"
#include "tgbot/types/ChatAdministratorRights.h"

#ifdef RTCOMMAND_LOADER
#include <RTCommandLoader.h>
#endif
#ifdef SOCKET_CONNECTION
#include <ChatObserver.h>
#include <SocketConnectionHandler.h>
#endif

#include <tgbot/tgbot.h>

// tgbot
using TgBot::TgLongPoll;

static void cleanupFn(int s) {
    static std::once_flag once;
    std::call_once(once, [s] {
        LOG_I("Exiting with signal %d", s);
        gSThreadManager.destroyManager();
        database::DBWrapper.save();
    });
    std::exit(0);
};

#ifdef SOCKET_CONNECTION
static void cleanupSocket(const std::string exitToken, SocketUsage usage,
                          SingleThreadCtrl *) {
    getSocketInterface(usage)->stopListening(exitToken);
}

static void setupSocket(const Bot &gBot,
                        SingleThreadCtrlManager::ThreadUsage tusage,
                        SocketUsage susage, TgBotCommandData::Exit e) {
    auto socketConnectionManager = gSThreadManager.getController(tusage);
    std::promise<bool> socketCreatedProm;
    auto socketCreatedFut = socketCreatedProm.get_future();
    auto intf = getSocketInterface(susage);

    if (!intf->isRunning) {
        socketConnectionManager->runWith([&socketCreatedProm, susage, &gBot] {
            getSocketInterface(susage)->startListening(
                [&gBot](struct TgBotConnection conn) {
                    socketConnectionHandler(gBot, conn);
                },
                socketCreatedProm);
        });

        if (socketCreatedFut.get()) {
            switch (susage) {
                case SocketUsage::SU_INTERNAL:
                    intf->writeToSocket({CMD_EXIT, {.data_2 = e}});
                    socketConnectionManager->setPreStopFunction(
                        std::bind(&cleanupSocket, e.token, susage,
                                  std::placeholders::_1));
                    break;
                case SocketUsage::SU_EXTERNAL:
                    socketConnectionManager->setPreStopFunction(
                        [intf](SingleThreadCtrl *) {
                            intf->forceStopListening();
                        });
                    break;
            };
        }
    }
}
#endif

int main(int argc, const char **argv) {
    std::string token;

    copyCommandLine(CommandLineOp::INSERT, &argc, &argv);
    if (ConfigManager::getVariable("help")) {
        ConfigManager::printHelp();
        return EXIT_SUCCESS;
    }
    auto ret = ConfigManager::getVariable("TOKEN");
    if (!ret) {
        LOG_F("Failed to get TOKEN variable");
        return EXIT_FAILURE;
    }
    token = *ret;
    static Bot gBot(token);
    database::DBWrapper.loadMain(gBot);

    bot_AddCommandEnforcedCompiler(
        gBot, "c", ProgrammingLangs::C,
        [](const Bot &bot, const Message::Ptr &message, std::string compiler) {
            static CompilerInTgForCCppImpl cCompiler(bot, compiler, "foo.c");
            cCompiler.run(message);
        });
    bot_AddCommandEnforcedCompiler(
        gBot, "cpp", ProgrammingLangs::CXX,
        [](const Bot &bot, const Message::Ptr &message, std::string compiler) {
            static CompilerInTgForCCppImpl cxxCompiler(bot, compiler, "foo.cpp");
            cxxCompiler.run(message);
        });
    bot_AddCommandEnforcedCompiler(
        gBot, "python", ProgrammingLangs::PYTHON,
        [](const Bot &bot, const Message::Ptr &message, std::string compiler) {
            static CompilerInTgForGenericImpl gPyCompiler(bot, compiler, "foo.py");
            gPyCompiler.run(message);
        });
    bot_AddCommandEnforcedCompiler(
        gBot, "golang", ProgrammingLangs::GO,
        [](const Bot &bot, const Message::Ptr &message, std::string compiler) {
            static CompilerInTgForGenericImpl GoCompiler(bot, compiler, "foo.go");
            GoCompiler.run(message);
        });

    for (const auto &i : gCmdModules) {
        if (i->isEnforced())
            bot_AddCommandEnforced(gBot, i->command, i->fn);
        else
            bot_AddCommandPermissive(gBot, i->command, i->fn);
    }
    gBot.getEvents().onAnyMessage([](const Message::Ptr &msg) {
        static auto spamMgr = gSThreadManager.getController<SpamBlockManager>(
            {SingleThreadCtrlManager::USAGE_SPAMBLOCK_THREAD}, std::ref(gBot));
        static RegexHandler regexHandler(gBot);

        if (!gAuthorized) return;
#ifdef SOCKET_CONNECTION
        if (!gObservedChatIds.empty() || gObserveAllChats)
            processObservers(msg);
#endif
        spamMgr->addMessage(msg);
        regexHandler.processRegEXCommandMessage(msg);
    });

#ifdef SOCKET_CONNECTION
    std::string exitToken = StringTools::generateRandomString(
        sizeof(TgBotCommandUnion::data_2.token) - 1);
    auto e = TgBotCommandData::Exit::create(ExitOp::SET_TOKEN, exitToken);

    setupSocket(gBot, SingleThreadCtrlManager::USAGE_SOCKET_THREAD, SU_INTERNAL,
                e);
    setupSocket(gBot, SingleThreadCtrlManager::USAGE_SOCKET_EXTERNAL_THREAD,
                SU_EXTERNAL, e);

#endif
#ifdef RTCOMMAND_LOADER
    loadCommandsFromFile(gBot, getSrcRoot() / "modules.load");
#endif
    installSignalHandler(cleanupFn);

    LOG_D("Token: %s", token.c_str());
    auto CurrentTp = std::chrono::system_clock::now();
    auto LastTp = std::chrono::system_clock::from_time_t(0);
    do {
        try {
            LOG_D("Bot username: %s", gBot.getApi().getMe()->username.c_str());
            gBot.getApi().deleteWebhook();

            TgLongPoll longPoll(gBot);
            while (true) {
                longPoll.start();
            }
        } catch (const std::exception &e) {
            LOG_E("Exception: %s", e.what());
            LOG_W("Trying to recover");
            UserId ownerid = database::DBWrapper.maybeGetOwnerId();
            try {
                bot_sendMessage(gBot, ownerid,
                                std::string("Exception occured: ") + e.what());
            } catch (const std::exception &e) {
                LOG_F("%s", e.what());
                break;
            }
            CurrentTp = std::chrono::system_clock::now();
            if (to_secs(CurrentTp - LastTp).count() < 30 &&
                std::chrono::system_clock::to_time_t(LastTp) != 0) {
                bot_sendMessage(gBot, ownerid, "Recover failed.");
                LOG_F("Recover failed");
                break;
            }
            LastTp = CurrentTp;
            bot_sendMessage(gBot, ownerid, "Reinitializing.");
            LOG_I("Re-init");
            gAuthorized = false;
            static const SingleThreadCtrlManager::GetControllerRequest req{
                .usage = SingleThreadCtrlManager::USAGE_ERROR_RECOVERY_THREAD,
                .flags =
                    SingleThreadCtrlManager::REQUIRE_NONEXIST |
                    SingleThreadCtrlManager::REQUIRE_FAILACTION_RETURN_NULL};
            auto cl = gSThreadManager.getController(req);
            if (cl) {
                cl->runWith([] {
                    std::this_thread::sleep_for(kErrorRecoveryDelay);
                    gAuthorized = true;
                });
            }
        }
    } while (true);
    cleanupFn(invalidSignal);
    return EXIT_FAILURE;
}
