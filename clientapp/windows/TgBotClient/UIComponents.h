#pragma once

#define _CRT_SECURE_NO_WARNINGS

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
DEFINE_COMPONENT(DownloadFileFn);

constexpr auto WM_SENDFILE_RESULT = (WM_USER + 1);
constexpr auto WM_SENDMSG_RESULT = (WM_USER + 2);
constexpr auto WM_DLFILE_RESULT = (WM_USER + 2);

constexpr auto ERROR_DIALOG = MB_ICONERROR | MB_OK;
constexpr auto WARNING_DIALOG = MB_ICONWARNING | MB_OK;
constexpr auto INFO_DIALOG = MB_ICONINFORMATION | MB_OK;

constexpr TCHAR kLineBreak[] = _T("\r\n");

constexpr INT_PTR DIALOG_OK = (INT_PTR)TRUE;
constexpr INT_PTR DIALOG_NO = (INT_PTR)FALSE;

template <size_t size>
inline void copyTo(std::array<char, size>& arr_in, const char* buf) {
    strncpy(arr_in.data(), buf, size);
}
template <size_t arrsize>
inline void copyTo(char* in_buf,
                   const std::array<char, arrsize>& arr_in, size_t size) {
    strncpy(in_buf, arr_in.data(), size - 1);
}

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

template <typename T>
T* allocMem() {
    T * mem = static_cast<T*>(malloc(sizeof(T)));
    if (mem) {
        ZeroMemory(mem, sizeof(T));
    }
    return mem;
}

inline std::wstring charToWstring(const char* charArray) {
    // Calculate the size of the buffer needed for the wide string
    size_t size = std::mbstowcs(nullptr, charArray, 0) + 1;

    // Allocate a wide string buffer
    std::wstring wstr(size, L'\0');

    // Perform the conversion
    std::mbstowcs(&wstr[0], charArray, size);

    return wstr;
}

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
