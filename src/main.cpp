#include <Authorization.h>
#include <BotAddCommand.h>
#include <ConfigManager.h>
#include <Database.h>
#include <FileSystemLib.h>
#include <RegEXHandler.h>
#include <SingleThreadCtrl.h>
#include <SpamBlock.h>
#include "signalhandler/SignalHandler.h"

// Generated cmd list
#include <cmds.gen.h>

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

template <class Dur>
std::chrono::seconds to_secs(Dur &&it) {
    return std::chrono::duration_cast<std::chrono::seconds>(it);
}

static void cleanupFn (int s) {
    static std::once_flag once;
    std::call_once(once, [s] {
        LOG_I("Exiting with signal %d", s);
        gSThreadManager.stopAll();
        database::DBWrapper.save();
    });
    std::exit(0);
};

#ifdef SOCKET_CONNECTION
static void cleanupSocket(const std::string exitToken, SingleThreadCtrl *) {
    bool socketValid = true;
    
    if (!fileExists(SOCKET_PATH)) {
        LOG_W("Socket file was deleted");
        socketValid = false;
    }
    if (socketValid) {
        writeToSocket({CMD_EXIT, {.data_2 = 
            TgBotCommandData::Exit::create(ExitOp::DO_EXIT, exitToken)}});
    } else {
        forceStopListening();
    }
}
#endif

int main(int argc, const char** argv) {
    std::string token, v;

    copyCommandLine(CommandLineOp::INSERT, &argc, &argv);
    if (ConfigManager::getVariable("help", v)) {
        ConfigManager::printHelp();
        return EXIT_SUCCESS;
    }
    if (!ConfigManager::getVariable("TOKEN", token)) {
        LOG_F("Failed to get TOKEN variable");
        return EXIT_FAILURE;
    }
    static Bot gBot(token);
    database::DBWrapper.load(/*withSync=*/ true);

    bot_AddCommandEnforcedCompiler(gBot, "c", ProgrammingLangs::C, [](const Bot &bot, const Message::Ptr &message, std::string compiler) {
        CompileRunHandler(CCppCompileHandleData{{{bot, message}, compiler, "out.c"}});
    });
    bot_AddCommandEnforcedCompiler(gBot, "cpp", ProgrammingLangs::CXX, [](const Bot &bot, const Message::Ptr &message, std::string compiler) {
        CompileRunHandler(CCppCompileHandleData{{{bot, message}, compiler, "out.cpp"}});
    });
    bot_AddCommandEnforcedCompiler(gBot, "python", ProgrammingLangs::PYTHON, [](const Bot &bot, const Message::Ptr &message, std::string compiler) {
        CompileRunHandler({{bot, message}, compiler, "out.py"});
    });
    bot_AddCommandEnforcedCompiler(gBot, "golang", ProgrammingLangs::GO, [](const Bot &bot, const Message::Ptr &message, std::string compiler) {
        CompileRunHandler({{bot, message}, compiler + " run", "out.go"});
    });

    for (const auto &i : gCmdModules) {
        if (i->enforced)
            bot_AddCommandEnforced(gBot, i->name, i->fn);
        else
            bot_AddCommandPermissive(gBot, i->name, i->fn);
    }
    gBot.getEvents().onAnyMessage([](const Message::Ptr &msg) {
        static auto spamMgr =  gSThreadManager
            .getController<SpamBlockManager>(SingleThreadCtrlManager::USAGE_SPAMBLOCK_THREAD);

        if (!gAuthorized) return;
#ifdef SOCKET_CONNECTION
        if (!gObservedChatIds.empty() || gObserveAllChats)
            processObservers(msg);
#endif
        spamMgr->run(gBot, msg);
        processRegEXCommand(gBot, msg);
    });

#ifdef SOCKET_CONNECTION
    auto socketConnectionManager = gSThreadManager
                                   .getController(SingleThreadCtrlManager::USAGE_SOCKET_THREAD);
    std::string exitToken;
    std::promise<bool> socketCreatedProm;
    auto socketCreatedFut = socketCreatedProm.get_future();

    socketConnectionManager->runWith([&socketCreatedProm] {
        startListening([](struct TgBotConnection conn) {
            socketConnectionHandler(gBot, conn);
        },
        socketCreatedProm);
    });

    if (socketCreatedFut.get()) {
        exitToken = StringTools::generateRandomString(sizeof(TgBotCommandUnion::data_2.token) - 1);

        auto e = TgBotCommandData::Exit::create(ExitOp::SET_TOKEN, exitToken);
        writeToSocket({CMD_EXIT, {.data_2 = e}});
        socketConnectionManager->setPreStopFunction(std::bind(&cleanupSocket,
            std::cref(exitToken), std::placeholders::_1));
    } else {
        gSThreadManager.destroyController(SingleThreadCtrlManager::USAGE_SOCKET_THREAD);
    }
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
                bot_sendMessage(gBot, ownerid, std::string("Exception occured: ") + e.what());
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
            auto cl = gSThreadManager.getController(SingleThreadCtrlManager::USAGE_ERROR_RECOVERY_THREAD,
                SingleThreadCtrlManager::FLAG_GETCTRL_REQUIRE_NONEXIST | 
                    SingleThreadCtrlManager::FLAG_GETCTRL_REQUIRE_NONEXIST_FAILACTION_IGNORE);
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
