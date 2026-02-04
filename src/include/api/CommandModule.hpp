#pragma once

#include <api/StringResLoader.hpp>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <set>
#include <string_view>
#include <type_traits>

#include "api/MessageExt.hpp"
#include "api/Providers.hpp"
#include "api/TgBotApi.hpp"

class CommandModule;

// Loading commandmodule definitions
#define DYN_COMMAND_SYM_STR "cmd"
#define DYN_COMMAND_SYM cmd

#ifdef _WIN32
#define DYN_COMMAND_EXPORT __declspec(dllexport)
#else  // #if __GNUC__ > 4 || defined __clang__ but C++20 codebase.
#define DYN_COMMAND_EXPORT [[gnu::visibility("default")]]
#endif

// Command handler helper macros
#define COMMAND_HANDLER_NAME(cmd) handle_command_##cmd
#define DECLARE_COMMAND_HANDLER(cmd)                                         \
    void COMMAND_HANDLER_NAME(cmd)(TgBotApi::Ptr api, MessageExt * message,  \
                                   const StringResLoader::PerLocaleMap* res, \
                                   const Providers* provider)

struct DynModule {
    using command_callback_t = void (*)(TgBotApi::Ptr api, MessageExt*,
                                        const StringResLoader::PerLocaleMap*,
                                        const Providers* provider);

    enum class Flags { None = 0, Enforced = 1 << 0, HideDescription = 1 << 1 };

    template <int... Ints>
    constexpr static bool are_all_unique() {
        // Helper lambda to check for duplicates
        constexpr int arr[] = {Ints...};
        for (std::size_t i = 0; i < sizeof...(Ints); ++i) {
            for (std::size_t j = i + 1; j < sizeof...(Ints); ++j) {
                if (arr[i] == arr[j]) {
                    return false;
                }
            }
        }
        return true;
    }

    using argcount_mask_t = int;
    // Crafts a mask of valid argument counts.
    template <int... N>
    constexpr static argcount_mask_t craftArgCountMask() noexcept {
        static_assert(are_all_unique<N...>(),
                      "All integers in the parameter pack must be unique.");

        return ((1 << N) | ...);
    }
    static std::set<int> fromArgCountMask(argcount_mask_t mask) {
        std::set<int> result;
        for (int i = 0; i < std::numeric_limits<argcount_mask_t>::digits; i++) {
            if ((mask & (1 << i)) != 0) {
                result.emplace(i);
            }
        }
        return result;
    }

    struct ValidArgs {
        // Is it enabled?
        bool enabled;
        // Int mask to the valid argument counts.
        argcount_mask_t counts;
        // Split type to obtain arguments.
        using Split = ::SplitMessageText;
        Split split_type;
        // Usage information for the command.
        // Optional
        const char* usage;
    };

    Flags flags;
    const char* name;
    const char* description;
    command_callback_t function;
    ValidArgs valid_args;
};

inline bool operator&(const DynModule::Flags& lhs,
                      const DynModule::Flags& rhs) {
    return (static_cast<int>(lhs) & static_cast<int>(rhs)) != 0;
}

constexpr DynModule::Flags operator|(const DynModule::Flags& lhs,
                                     const DynModule::Flags& rhs) noexcept {
    return static_cast<DynModule::Flags>(static_cast<int>(lhs) |
                                         static_cast<int>(rhs));
}

// Command Module Base
class CommandModule {
   public:
    using Ptr = std::unique_ptr<CommandModule>;
    using command_callback_t =
        std::function<std::remove_pointer_t<DynModule::command_callback_t>>;

    // Module information.
    struct Info {
        std::string name;
        std::string description;
        DynModule::Flags flags;
        command_callback_t function;
        struct ValidArgs {
            // Is it enabled?
            bool enabled;
            // Int mask to the valid argument counts.
            DynModule::argcount_mask_t counts;
            // Split type to obtain arguments.
            ::SplitMessageText split_type;
            // Usage information for the command.
            std::string usage;
        } valid_args;
        enum class Type {
            None,       // Unknown
            SharedLib,  // .so based traditional
            Lua         // Lua script
        } module_type;

        explicit Info(const DynModule* dyn)
            : name(dyn->name),
              description(dyn->description),
              flags(dyn->flags),
              function(dyn->function),
              module_type(Type::SharedLib) {
            valid_args.enabled = dyn->valid_args.enabled;
            if (!valid_args.enabled) {
                return;
            }

            valid_args.counts = dyn->valid_args.counts;
            valid_args.split_type = dyn->valid_args.split_type;
            if (dyn->valid_args.usage) valid_args.usage = dyn->valid_args.usage;
        }
        Info() = default;

        // Trival accessors.
        [[nodiscard]] bool isPrivileged() const {
            return flags & DynModule::Flags::Enforced;
        }
        [[nodiscard]] bool isHideDescription() const {
            return flags & DynModule::Flags::HideDescription;
        }
    } info;

    /**
     * @brief Loads the command module from the specified file path.
     *
     * This function attempts to load the command module from the given file
     * path.
     *
     * @return true if the command module is successfully loaded; false
     * otherwise.
     */
    virtual bool load() = 0;

    /**
     * @brief Unloads the command module.
     *
     * @return true if the command module is successfully unloaded; false
     * otherwise.
     */
    virtual bool unload() = 0;

    // Trival accessors.
    [[nodiscard]] virtual bool isLoaded() const = 0;

    virtual ~CommandModule() = default;
};

// .so based (binary) command module
class DynCommandModule : public CommandModule {
   public:
    using Ptr = std::unique_ptr<DynCommandModule>;

    static constexpr std::string_view prefix = "libcmd_";

   private:
    // dlclose RAII handle
    std::unique_ptr<void, int (*)(void*)> handle;
    std::filesystem::path filePath;

    /**
     * @brief Constructs a new instance of DynCommandModule.
     *
     * This constructor initializes a new DynCommandModule object with the given
     * file path. The filePath parameter specifies the path to the shared
     * library file containing the command module implementation.
     *
     * @param filePath The path to the shared library file containing the
     * command module implementation.
     */
   public:
    explicit DynCommandModule(std::filesystem::path filePath);

    /**
     * @brief Loads the command module from the specified file path.
     *
     * This function attempts to load the command module from the given file
     * path. It opens the shared library file, retrieves the function pointer
     * for the dynamic command symbol (DYN_COMMAND_SYM), and initializes the
     * command module.
     *
     * @return true if the command module is successfully loaded; false
     * otherwise.
     */
    bool load() override;

    /**
     * @brief Unloads the command module.
     *
     * This function unloads the command module by closing the shared library
     * file and releasing any allocated resources.
     *
     * @return true if the command module is successfully unloaded; false
     * otherwise.
     */
    bool unload() override;

    // Trival accessors.
    [[nodiscard]] bool isLoaded() const override;

    ~DynCommandModule() override = default;
};

#ifdef HAVE_LUA
// .lua based command module
class LuaCommandModule : public CommandModule {
   public:
    using Ptr = std::unique_ptr<LuaCommandModule>;

   private:
    struct Context;
    std::unique_ptr<Context> _context;

    /**
     * @brief Constructs a new instance of LuaCommandModule.
     *
     * This constructor initializes a new LuaCommandModule object with the given
     * file path. The filePath parameter specifies the path to the script file
     * containing the command module implementation.
     *
     * @param filePath The path to the script file containing the
     * command module implementation.
     */
   public:
    explicit LuaCommandModule(std::filesystem::path filePath);

    /**
     * @brief Loads the command module from the specified file path.
     *
     * This function attempts to load the command module from the given file
     * path. It opens the script file, initializes the
     * command module.
     *
     * @return true if the command module is successfully loaded; false
     * otherwise.
     */
    bool load() override;

    /**
     * @brief Unloads the command module.
     *
     * @return true if the command module is successfully unloaded; false
     * otherwise.
     */
    bool unload() override;

    // Trival accessors.
    [[nodiscard]] bool isLoaded() const override;

    ~LuaCommandModule() override;
};
#endif  // defined HAVE_LUA

class BuiltinCommandModule : public CommandModule {
   public:
    using Ptr = std::unique_ptr<BuiltinCommandModule>;

   private:
    bool loaded{};

   public:
    explicit BuiltinCommandModule(const DynModule* dyn) { info = Info(dyn); }

    bool load() override {
        loaded = true;
        return true;
    }

    bool unload() override {
        loaded = false;
        return true;
    }

    // Trival accessors.
    [[nodiscard]] bool isLoaded() const override { return loaded; }
};