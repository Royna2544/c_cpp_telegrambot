#include <cstdlib>
#include <string>

bool getHomePath(std::string& buf) {
    auto buf_c = getenv("HOME");
    if (buf_c) {
        buf = buf_c;
    }
    return !!buf_c;
}
