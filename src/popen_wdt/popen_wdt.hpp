#include <string>
#include <filesystem>

/**
 * runCommand - Runs a command and store result into buffer
 *
 * @param command - Command to exec
 * @param res - Out buffer storage
 * @return If executing process succeeded
 */
[[deprecated]]
bool runCommand(const std::string& command, std::string& res);

/**
 * getSrcRoot - Fetch source root directory with git command
 *
 * @return source root directory path
 */
[[deprecated("To be removed in the future. Use libfs::getSrcRoot() instead")]]
std::filesystem::path getSrcRoot();