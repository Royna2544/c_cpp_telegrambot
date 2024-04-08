#include <SocketConnectionHandler.h>
#include <socket/TgBotSocket.h>

#include <socket/getter/SocketInterfaceGetter.hpp>

#include "CStringLifetime.h"
#include "initcalls/BotInitcall.hpp"

struct SocketInterfaceInit : BotInitCall {
    void doInitCall(Bot &bot) override {
        std::string exitToken =
            StringTools::generateRandomString(TgBotCommandData::Exit::TokenLen);
        auto e = TgBotCommandData::Exit::create(ExitOp::SET_TOKEN, exitToken);
        auto p = std::make_shared<SocketInterfacePriv>();
        auto inter = SocketInterfaceGetter::get(
            SocketInterfaceGetter::typeForInternal,
            SocketInterfaceGetter::SocketUsage::USAGE_INTERNAL);

        auto exter = SocketInterfaceGetter::get(
            SocketInterfaceGetter::typeForExternal,
            SocketInterfaceGetter::SocketUsage::USAGE_EXTERNAL);

        p->listener_callback = [&bot](struct TgBotConnection conn) {
            socketConnectionHandler(bot, conn);
        };
        p->e = std::move(e);

        for (auto &intf : {inter, exter}) {
            intf->setPriv(p);
            intf->run();
        }
    }
    const CStringLifetime getInitCallName() const override {
        return "Create sockets and setup";
    }
};
