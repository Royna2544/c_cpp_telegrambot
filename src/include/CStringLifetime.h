#include <string>

struct CStringLifetime {
    CStringLifetime() = default;
    CStringLifetime(const std::string&& other) { onNewStr(std::move(other)); }
    void operator=(const std::string&& other) { onNewStr(std::move(other)); }
    const char* get() const { return _c_str; }

   private:
    void onNewStr(const std::string&& other) {
        _str = other;
        _c_str = _str.c_str();
    }
    std::string _str;
    const char* _c_str = nullptr;
};