#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string_view>

#include "StringResLoader.hpp"
#include "api/MessageExt.hpp"
#include "api/TgBotApi.hpp"
#include "api/Providers.hpp"

class CommandModule;

// Loading commandmodule definitions
using loadcmd_function_cstyle_t = bool (*)(const std::string_view,
                                           CommandModule&);
using loadcmd_function_t =
    std::function<std::remove_pointer_t<loadcmd_function_cstyle_t>>;

#define DYN_COMMAND_SYM_STR "loadcmd"
#define DYN_COMMAND_SYM loadcmd
#define DYN_COMMAND_FN(n, m) \
    extern "C" bool DYN_COMMAND_SYM(const std::string_view n, CommandModule&(m))

// Command handler helper macros
#define COMMAND_HANDLER_NAME(cmd) handle_command_##cmd
#define DECLARE_COMMAND_HANDLER(cmd)                \
    void COMMAND_HANDLER_NAME(cmd)(                 \
        TgBotApi::Ptr api, MessageExt::Ptr message, \
        const StringResLoaderBase::LocaleStrings* res, const Providers* provider)

using command_callback_t = std::function<void(
    TgBotApi::Ptr api, MessageExt::Ptr,
    const StringResLoaderBase::LocaleStrings*, const Providers* provider)>;

class CommandModule {
   public:
    using Ptr = std::unique_ptr<CommandModule>;

    static constexpr std::string_view prefix = "libcmd_";

    // Module information.
    enum Flags { None = 0, Enforced = 1 << 0, HideDescription = 1 << 1 };
    command_callback_t function;
    unsigned int flags = None;
    std::string name;
    std::string description;

    struct ValidArgs {
        // Is it enabled?
        bool enabled;
        // Int array to the valid argument count array.
        std::vector<int> counts;
        // Split type to obtain arguments.
        using Split = ::SplitMessageText;
        Split split_type;
        // Usage information for the command.
        std::string usage;
    } valid_arguments{};

   private:
    // dlclose RAII handle
    std::unique_ptr<void, int (*)(void*)> handle;
    std::filesystem::path filePath;

    /**
     * @brief Constructs a new instance of CommandModule.
     *
     * This constructor initializes a new CommandModule object with the given
     * file path. The filePath parameter specifies the path to the shared
     * library file containing the command module implementation.
     *
     * @param filePath The path to the shared library file containing the
     * command module implementation.
     */
   public:
    explicit CommandModule(std::filesystem::path filePath);

    /**
     * @brief Destroys the CommandModule instance.
     *
     * The destructor releases any resources held by the CommandModule object.
     * It sets the function pointer to nullptr to prevent any dangling
     * references.
     */
    ~CommandModule() { function = nullptr; }

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
    bool load();

    /**
     * @brief Unloads the command module.
     *
     * This function unloads the command module by closing the shared library
     * file and releasing any allocated resources.
     *
     * @return true if the command module is successfully unloaded; false
     * otherwise.
     */
    bool unload();

    // Trival accessors.
    [[nodiscard]] bool isLoaded() const { return handle != nullptr; }
    [[nodiscard]] bool isEnforced() const { return (flags & Enforced) != 0; }
    [[nodiscard]] bool isHideDescription() const {
        return (flags & HideDescription) != 0;
    }
};