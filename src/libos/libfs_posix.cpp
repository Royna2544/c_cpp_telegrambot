#include <cstdlib>
#include <filesystem>

bool getHomePath(std::filesystem::path& buf) {
    auto buf_c = getenv("HOME");
    if (buf_c) {
        buf = buf_c;
    }
    return !!buf_c;
}
