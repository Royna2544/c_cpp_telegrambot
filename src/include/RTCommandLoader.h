#pragma once

#include <TgBotPPImplExports.h>
#include <tgbot/Bot.h>

#include <filesystem>
#include <vector>

#include "CStringLifetime.h"
#include "InstanceClassBase.hpp"
#include "initcalls/Initcall.hpp"

using TgBot::Bot;
using TgBot::Message;

struct TgBotPPImpl_API DynamicLibraryHolder {
    explicit DynamicLibraryHolder(void* handle) : handle_(handle){};
    DynamicLibraryHolder(DynamicLibraryHolder&& other) noexcept;
    ~DynamicLibraryHolder();

   private:
    void* handle_;
};

struct TgBotPPImpl_API RTCommandLoader
    : public InstanceClassBase<RTCommandLoader>,
      InitCall {
    virtual ~RTCommandLoader() = default;
    RTCommandLoader() = default;

    /**
     * @brief loads a single command from a file
     * @param bot the bot instance
     * @param fname the file path of the command
     */
    bool loadOneCommand(std::filesystem::path fname);

    void doInitCall() override;
    const CStringLifetime getInitCallName() const override {
        return "Load commands from shared libraries";
    }

   private:
    std::vector<DynamicLibraryHolder> libs;
};
