#pragma once

#include <json/json.h>

#include <api/TgBotApi.hpp>

// Uses VirusTotal API to check virus files.
class FileCheck {
    std::string _token;
   public:
    explicit FileCheck(TgBotApi::Ptr api, std::string virusTotalToken);
    TgBotApi::AnyMessageResult onAnyMessage(TgBotApi::CPtr api, const Message::Ptr &message);
};