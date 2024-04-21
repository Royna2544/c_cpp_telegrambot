#include "SocketBase.hpp"
#include <absl/log/log.h>

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
            LOG(FATAL) << "Option is not set, and trying to get it: option "
                       << static_cast<int>(opt);
        }
        ret = option->data;
        if (!option->persistent) option.reset();
    }
    return ret;
}

SocketInterfaceBase::option_t* SocketInterfaceBase::getOptionPtr(Options p) {
    option_t* optionVal = nullptr;
    switch (p) {
        case Options::DESTINATION_ADDRESS:
            optionVal = &options.opt_destination_address;
            break;
        case Options::DESTINATION_PORT:
            optionVal = &options.opt_destination_port;
            break;
    }
    return optionVal;
}
