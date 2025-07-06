#include <absl/log/log.h>
#include <absl/strings/strip.h>
#include <fmt/format.h>

#include <GitBuildInfo.hpp>
#include <api/CommandModule.hpp>
#include <memory>
#include <sol/sol.hpp>
#include <string_view>

struct LuaCommandModule::Context {
    sol::state lua;
    std::filesystem::path filePath;
};

LuaCommandModule::LuaCommandModule(std::filesystem::path filePath)
    : _context(std::make_unique<Context>()) {
    _context->lua.open_libraries(sol::lib::base);
    _context->filePath = std::move(filePath);
}

LuaCommandModule::~LuaCommandModule() = default;

bool LuaCommandModule::load() {
    // Load script file
    _context->lua.script_file(_context->filePath.string());
    // Make bindings
    sol::table meta = _context->lua["command"];
    info.name = meta["name"];
    info.description = meta["description"];
    bool permissive = meta.get_or("permissive", false);
    bool hide_description = meta.get_or("hide_description", false);

    DynModule::Flags flags{};
    if (!permissive) {
        flags = flags | DynModule::Flags::Enforced;
    }
    if (!hide_description) {
        flags = flags | DynModule::Flags::HideDescription;
    }
    info.flags = flags;

    info.function = [c = _context.get()](
                        TgBotApi::Ptr api, MessageExt* message,
                        const StringResLoader::PerLocaleMap* res,
                        const Providers* provider) {
        c->lua["reply"] = [api, message](const std::string_view text) {
            api->sendReplyMessage(message->message(), text);
        };

        // bind db helper
        sol::table db = c->lua.create_table();
        db.set_function(
            "get", [provider](const std::string& k, sol::this_state L) -> sol::object {
                if (k == "owner_id") {
                    auto val = provider->database->getOwnerUserId();
                    return val ? sol::make_object(L, *val) : sol::nil;
                }
                return sol::nil;
            });
        db.set_function("set", [provider](const std::string& k, sol::object v) {
            if (v.is<int>())
                if (k == "owner_id") provider->database->setOwnerUserId(v.as<int>());
        });
        c->lua["db"] = db;

        c->lua["run"]();
    };

    DLOG(INFO) << "Loaded Lua script: " << _context->filePath;

    return true;
}

bool LuaCommandModule::unload() { return false; }

bool LuaCommandModule::isLoaded() const { return true; }
