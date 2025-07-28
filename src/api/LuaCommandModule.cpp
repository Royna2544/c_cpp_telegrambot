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
    bool isLoaded = false;
};

LuaCommandModule::LuaCommandModule(std::filesystem::path filePath)
    : _context(std::make_unique<Context>()) {
    _context->filePath = std::move(filePath);
    info.module_type = Info::Type::Lua;
}

LuaCommandModule::~LuaCommandModule() = default;

void binds(TgBotApi::Ptr api, MessageExt* message,
           const StringResLoader::PerLocaleMap* res, const Providers* provider,
           sol::state_view& lua) {
    // bind message object
    /*───────────────────  message table  ───────────────────*/
    sol::table msg = lua.create_table();
    msg["chat_id"] = message->get<MessageAttrs::Chat>()->id;
    msg["message_id"] = message->get<MessageAttrs::MessageId>();
    msg["date"] = message->message()->date;  // seconds since epoch
    lua["message"] = msg;

    /*───────────────────  time helpers  ────────────────────*/
    using namespace std::chrono;
    lua["now"] = []() -> double {
        return duration_cast<seconds>(system_clock::now().time_since_epoch())
            .count();
    };
    lua["now_ms"] = []() -> std::uint64_t {
        return duration_cast<milliseconds>(
                   steady_clock::now().time_since_epoch())
            .count();
    };

    /*───────────────────  bot helpers  ─────────────────────*/
    lua["reply"] = [api, message, &lua](const std::string& txt) {
        auto m = api->sendReplyMessage(message->message(), txt);

        sol::table t = lua.create_table();
        t["chat_id"] = m->chat->id;
        t["message_id"] = m->messageId;
        t["date"] = static_cast<double>(m->date);
        return t;  // returned to Lua
    };

    lua["edit"] = [api](sol::table m, const std::string& txt) {
        TgBot::Message::Ptr ph = std::make_shared<TgBot::Message>();
        ph->chat = std::make_shared<TgBot::Chat>();
        ph->chat->id = m["chat_id"];
        ph->messageId = m["message_id"];
        api->editMessage(ph, txt);
    };

    // bind db helper
    sol::table db = lua.create_table();
    db.set_function(
        "get",
        [provider](const std::string& k, sol::this_state L) -> sol::object {
            if (k == "owner_id") {
                auto val = provider->database->getOwnerUserId();
                return val ? sol::make_object(L, *val) : sol::lua_nil;
            }
            return sol::lua_nil;
        });
    db.set_function("set", [provider](const std::string& k, sol::object v) {
        if (v.is<int>())
            if (k == "owner_id")
                provider->database->setOwnerUserId(v.as<int>());
    });
    lua["db"] = db;
}

bool LuaCommandModule::load() {
    _context->lua.open_libraries(sol::lib::base, sol::lib::string,
                                 sol::lib::os);
    sol::state lua;

    DLOG(INFO) << "Load file: " << _context->filePath.filename();
    // Load script file
    try {
        _context->lua.script_file(_context->filePath.string());
    } catch (const sol::error& ex) {
        LOG(ERROR) << ex.what();
        return false;
    }

    // Grab module info
    sol::table meta;

    auto obj = _context->lua["command"];
    if (!obj.is<sol::table>()) {
        LOG(ERROR) << "Variable 'command' is not a table";
        return false;
    }
    meta = obj;
    try {
        info.name = meta["name"];
        info.description = meta["description"];
    } catch (const sol::error& ex) {
        LOG(ERROR) << "Missing command.name command.description entries";
        return false;
    }
    bool permissive = meta.get_or("permissive", false);
    bool hide_description = meta.get_or("hide_description", false);

    DynModule::Flags flags{};
    if (!permissive) {
        flags = flags | DynModule::Flags::Enforced;
    }
    if (hide_description) {
        flags = flags | DynModule::Flags::HideDescription;
    }
    info.flags = flags;

    info.function = [c = _context.get()](
                        TgBotApi::Ptr api, MessageExt* message,
                        const StringResLoader::PerLocaleMap* res,
                        const Providers* provider) {
        sol::state_view lua = c->lua;

        if (!c->isLoaded) return;

        binds(api, message, res, provider, lua);

        try {
            lua["run"]();
        } catch (const sol::error& ex) {
            LOG(ERROR) << ex.what();
            api->sendReplyMessage(message->message(),
                                  res->get(Strings::BACKEND_ERROR));
        }
    };

    DLOG(INFO) << "Loaded Lua script: " << _context->filePath;

    _context->isLoaded = true;
    return true;
}

bool LuaCommandModule::unload() {
    _context->lua = {};
    _context->isLoaded = false;
    return true;
}

bool LuaCommandModule::isLoaded() const { return _context->isLoaded; }
