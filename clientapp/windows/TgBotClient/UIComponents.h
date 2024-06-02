#pragma once

#include <Windows.h>
#include <tchar.h>

#include <array>
#include <map>
#include <string>

#include "resource.h"


#define DEFINE_COMPONENT(name) INT_PTR CALLBACK name(HWND, UINT, WPARAM, LPARAM)

DEFINE_COMPONENT(SendMsgToChat);
DEFINE_COMPONENT(SendFileToChat);
DEFINE_COMPONENT(About);
DEFINE_COMPONENT(Uptime);
DEFINE_COMPONENT(DestinationIP);

class StringLoader {
   public:
    constexpr static size_t MAX_LEN = 256;
    using String = std::basic_string<TCHAR>;

    static void initInstance(HINSTANCE hAppInst) {
        instance.hAppInst = hAppInst;
    }
    static StringLoader& getInstance() { return instance; }

    String getString(const int id) {
        if (cachedStr.find(id) != cachedStr.end()) {
            return cachedStr.at(id);
        }
        std::array<TCHAR, MAX_LEN> buffer{};
        LoadString(hAppInst, id, buffer.data(), MAX_LEN - 1);
        cachedStr[id] = buffer.data();
        return buffer.data();
    }

   private:
    HINSTANCE hAppInst;
    std::map<int, String> cachedStr;
    static StringLoader instance;
};

inline const TCHAR* kLineBreak = _T("\r\n");
constexpr INT_PTR DIALOG_OK = (INT_PTR)TRUE;
constexpr INT_PTR DIALOG_NO = (INT_PTR)FALSE;

template <typename CharT>
std::size_t Length(CharT* str) = delete;

template <>
inline std::size_t Length(char* str) {
    return std::strlen(str);
}
template <>
inline std::size_t Length(wchar_t* str) {
    return std::wcslen(str);
}