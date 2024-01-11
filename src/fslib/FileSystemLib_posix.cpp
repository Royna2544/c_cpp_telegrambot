#include <unistd.h>

#include <cstdlib>
#include <string>

bool getHomePath(std::string& buf) {
    auto buf_c = getenv("HOME");
    if (buf_c) {
        buf = buf_c;
    }
    return !!buf_c;
}
bool canExecute(const std::string& path) {
    return access(path.c_str(), R_OK | X_OK) == 0;
}
