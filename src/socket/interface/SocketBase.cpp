#include "SocketBase.hpp"

#include <absl/log/log.h>

#include <optional>
#include <utility>

#include "SocketDescriptor_defs.hpp"

void SocketInterfaceBase::setOptions(Options opt, const std::string data,
                                     bool persistent) {
    option_t* optionVal = getOptionPtr(opt);
    if (optionVal != nullptr) {
        OptionContainer c;
        c.data = data;
        c.persistent = persistent;
        *optionVal = c;
    }
}

std::string SocketInterfaceBase::getOptions(Options opt) {
    std::string ret;
    option_t* optionVal = getOptionPtr(opt);
    if (optionVal != nullptr) {
        option_t option = *optionVal;
        if (!option.has_value()) {
            LOG(WARNING) << "Option is not set, and trying to get it: option "
                         << static_cast<int>(opt);
            throw std::bad_optional_access();
        }
        ret = option->data;
        if (!option->persistent) {
            option.reset();
        }
    }
    return ret;
}

SocketInterfaceBase::option_t* SocketInterfaceBase::getOptionPtr(Options opt) {
    option_t* optionVal = nullptr;
    switch (opt) {
        case Options::DESTINATION_ADDRESS:
            optionVal = &options.opt_destination_address;
            break;
        case Options::DESTINATION_PORT:
            optionVal = &options.opt_destination_port;
            break;
    }
    return optionVal;
}

void SocketInterfaceBase::writeAsClientToSocket(SocketData data) {
    const auto handle = createClientSocket();
    if (handle.has_value()) {
        writeToSocket(handle.value(), std::move(data));
    }
}

void SocketInterfaceBase::startListeningAsServer(
    const listener_callback_t onNewData) {
    auto hdl = createServerSocket();
    if (hdl) {
        startListening(hdl.value(), onNewData);
    }
}

bool SocketInterfaceBase::closeSocketHandle(SocketConnContext& context) {
    return closeSocketHandle(context.cfd);
}
