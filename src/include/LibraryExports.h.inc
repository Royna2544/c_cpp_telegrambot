#ifndef TGBOTPP_DLL
#define TGBOTPP_DLL
#if defined _WIN32 || defined __CYGWIN__
#define TGBOTPP_HELPER_DLL_EXPORT __declspec(dllexport)
#define TGBOTPP_HELPER_DLL_IMPORT __declspec(dllimport)
#else
#if __GNUC__ >= 4
#define TGBOTPP_HELPER_DLL_EXPORT __attribute__((visibility("default")))
#define TGBOTPP_HELPER_DLL_IMPORT __attribute__((visibility("default")))
#else
#define TGBOTPP_HELPER_DLL_EXPORT
#define TGBOTPP_HELPER_DLL_IMPORT
#endif
#endif
#endif

#ifdef @TGBOT_LIBRARY_NAME@_EXPORTS
#define @TGBOT_LIBRARY_NAME@_API TGBOTPP_HELPER_DLL_EXPORT
#else
#define @TGBOT_LIBRARY_NAME@_API TGBOTPP_HELPER_DLL_IMPORT
#endif