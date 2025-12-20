#pragma once

#include <absl/status/status.h>

#include <expected>
#include <memory>
#include <regex>
#include <string>
#include <mutex>

/**
 * @brief Interface for a regex command.
 *
 * This interface defines the methods that a regex command should implement.
 * The derived classes should provide the regex pattern for the command,
 * a brief description of the command, and a method to process the command.
 */
class RegexCommand {
   public:
    RegexCommand() = default;
    virtual ~RegexCommand() = default;

    enum class Error {
        InvalidRegexMatchIndex,  // in s/src/dest/g3 for 'srcsrc' for example.
        InvalidRegexOption,      // Unknown regex option.
        // Using a global flag and a match index doesn't make sense.
        GlobalFlagAndMatchIndexInvalid,
        InvalidRegex, // Regex command itself is invalid
        None,  // Regex didn't match, so just ignore.
    };

    using Result = std::expected<std::string, RegexCommand::Error>;

    /**
     * @brief Returns a brief description of the command.
     *
     * @return A brief description of the command.
     */
    [[nodiscard]] virtual std::string_view description() const = 0;
    
   protected:
    /**
     * @brief Returns the regex pattern for the command.
     *
     * @return The regex pattern for the command.
     */
    [[nodiscard]] virtual std::regex command_regex() const = 0;


    /**
     * @brief Processes the command using the given source and command strings.
     *
     * @param source The source string on which the command will be applied.
     * @param command The command string to be processed.
     *
     * @return The result of processing the command.
     */
    [[nodiscard]] virtual Result process(const std::string& source,
                                         const std::smatch command) const = 0;

   public:
    [[nodiscard]] Result process(const std::string& source,
                                 const std::string& regexCommand) const;
};

struct RegexHandler {
    RegexHandler();
    ~RegexHandler() = default;

    struct Interface {
        virtual ~Interface() = default;
        // Called when regex processing is complete. Error or success
        virtual void onError(const absl::Status& status) = 0;
        virtual void onSuccess(const std::string& result) = 0;
    };

    /**
     * @brief Registers a regex command handler with the RegexHandler.
     *
     * This function adds a unique pointer to a RegexCommand object to the
     * internal handlers vector. The ownership of the RegexCommand object is
     * transferred to the RegexHandler.
     *
     * @param handler A unique pointer to a RegexCommand object.
     */
    void registerCommand(std::unique_ptr<RegexCommand> handler);

    /**
     * @brief Executes the registered regex commands and notifies the callback.
     *
     * This function iterates through all the registered regex command handlers,
     * processes the commands, and notifies the provided callback interface with
     * the result or error status.
     *
     * @param callback A shared pointer to an Interface object that will receive
     *                 the result or error status.
     * @param source The source string on which the commands will be applied.
     * @param regexCommand The command string to be processed.
     */
    void execute(const std::shared_ptr<Interface>& callback,
                 const std::string& source, const std::string& regexCommand);

   private:
    std::vector<std::unique_ptr<RegexCommand>> _handlers;
    std::mutex _mutex;
};
