#include <libxml/parser.h>

#include <string>
#include <trivial_helpers/raii.hpp>

class XmlCharWrapper {
    RAII<xmlChar *>::Value<void> string;

   public:
    XmlCharWrapper(xmlChar *buf) : string(buf, xmlFree) {}
    XmlCharWrapper() = default;

    operator xmlChar *() const noexcept { return string.get(); }

    bool operator==(const std::string_view other) const noexcept {
        if (c_str() == nullptr) return false;
        return c_str() == other;
    }
    bool operator==(const XmlCharWrapper &other) const noexcept {
        return operator==(other.string.get());
    }
    bool operator==(const xmlChar *other) const noexcept {
        if (!string || !other) return false;
        return xmlStrcmp(string.get(), other) == 0;
    }
    bool operator!=(const XmlCharWrapper &other) const noexcept {
        return !operator==(other);
    }
    const char *c_str() const noexcept {
        return reinterpret_cast<const char *>(string.get());
    }
    std::string str() const { return {c_str()}; }
};

XmlCharWrapper operator""_xmlChar(const char *string, size_t length) {
    return {xmlCharStrdup(string)};
}

// libxml2 error handle function
struct libxml2_error_ctx {
    int code{};
    std::string message;
};

inline void libxml2_error_handler(void *ctx, const char *msg, ...) {
    va_list args;
    std::array<char, 256> errorBuffer{};
    va_start(args, msg);
    vsnprintf(errorBuffer.data(), errorBuffer.size(), msg, args);
    va_end(args);
    auto *error_ctx = static_cast<libxml2_error_ctx *>(ctx);
    error_ctx->code = xmlGetLastError()->code;
    error_ctx->message += errorBuffer.data();
}
