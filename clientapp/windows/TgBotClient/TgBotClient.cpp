#define _CRT_SECURE_NO_WARNINGS

#pragma comment(lib, "Shcore")

#include <absl/log/log.h>
#include <shellscalingapi.h>  // DPI
#include <shlobj.h>
#include <shlwapi.h>

#include <boost/program_options.hpp>
#include <filesystem>
#include <fstream>

#include "AbseilLogging.h"
#include "UIComponents.h"
#include "TgBotSocketIntf.h"


// 전역 변수:
HINSTANCE hInst;  // 현재 인스턴스입니다.
std::wstring szTitle;

namespace po = boost::program_options;

namespace OptParsing {

constexpr static std::string_view kConfigFile = "tgbotclient.ini";
constexpr static std::string_view kIPAddressConfig = "IP_ADDRESS";
constexpr static std::string_view kUseINet6Config = "USE_IPV6";
constexpr static std::string_view kPortNumConfig = "PORT_NUM";

static po::variables_map mp;
static po::options_description createDesc() {
    po::options_description desc;
    desc.add_options()(kIPAddressConfig.data(), po::value<std::string>(),
                       "Remote IP Address");
    desc.add_options()(kPortNumConfig.data(), po::value<int>(),
                       "Port number where TgBot++ server is listening to");
    desc.add_options()(kUseINet6Config.data(), po::value<bool>(),
                       "Use IPV6 to connect server");
    return desc;
}

void loadAndUpdateSocketConfig(std::filesystem::path directory) {
    std::filesystem::path configFilePath;
    configFilePath = std::filesystem::path(directory) / kConfigFile;
    DLOG(INFO) << "Trying config file path: " << configFilePath;
    std::ifstream configFileStream(configFilePath);
    if (configFileStream) {
        bool success = true;
        DLOG(INFO) << "Success on opening config file";
        try {
            po::store(po::parse_config_file(configFileStream, createDesc()),
                      mp);
        } catch (const po::error &e) {
            LOG(ERROR) << "Failed to parse config file: " << e.what();
            success = false;
        }
        if (success) {
            po::notify(mp);

            SocketConfig config{};
            config.mode = SocketConfig::Mode::USE_IPV4;
            config.port = 50000;

            if (const auto it = mp[kUseINet6Config.data()]; !it.empty()) {
                if (it.as<bool>()) {
                    config.mode = SocketConfig::Mode::USE_IPV6;
                }
            }
            if (const auto it = mp[kIPAddressConfig.data()]; !it.empty()) {
                config.address = it.as<std::string>();
            }
            if (const auto it = mp[kPortNumConfig.data()]; !it.empty()) {
                config.port = it.as<int>();
            }
            setSocketConfig(config);
        }
    } else {
        LOG(WARNING) << "Failed to open file";
    }
}
}

// 이 코드 모듈에 포함된 함수의 선언을 전달합니다:
ATOM MyRegisterClass(HINSTANCE hInstance);
BOOL InitInstance(HINSTANCE, int);
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance, _In_ LPWSTR lpCmdLine,
                      _In_ int nCmdShow) {
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    StringLoader::initInstance(hInstance);

    szTitle = StringLoader::getInstance().getString(IDS_APP_TITLE);
    MyRegisterClass(hInstance);

    // 애플리케이션 초기화를 수행합니다:
    if (!InitInstance(hInstance, nCmdShow)) {
        LOG(ERROR) << "Failed to init instance, returning instantly";
        return FALSE;
    }

    // Logging init
    initLogging();

    CHAR userDir[MAX_PATH];
    bool ret =
        SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PROFILE, NULL, 0, userDir));
    if (ret) {
        OptParsing::loadAndUpdateSocketConfig(userDir);
    }

    // DPI Scaling
    SetProcessDpiAwareness(PROCESS_SYSTEM_DPI_AWARE);

    HACCEL hAccelTable =
        LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_TGBOTCLIENT));
    MSG msg;
    // 기본 메시지 루프입니다:
    while (GetMessage(&msg, nullptr, 0, 0)) {
        if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return (int)msg.wParam;
}

//
//  함수: MyRegisterClass()
//
//  용도: 창 클래스를 등록합니다.
//
ATOM MyRegisterClass(HINSTANCE hInstance) {
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_TGBOTCLIENT));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_MENU);
    wcex.lpszClassName = szTitle.c_str();
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_TGBOTCLIENT));

    return RegisterClassExW(&wcex);
}

//
//   함수: InitInstance(HINSTANCE, int)
//
//   용도: 인스턴스 핸들을 저장하고 주 창을 만듭니다.
//
//   주석:
//
//        이 함수를 통해 인스턴스 핸들을 전역 변수에 저장하고
//        주 프로그램 창을 만든 다음 표시합니다.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow) {
    hInst = hInstance;  // 인스턴스 핸들을 전역 변수에 저장합니다.

    HWND hWnd = CreateWindowW(
        szTitle.c_str(), szTitle.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, 0,
        CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (hWnd == nullptr) {
        return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}

//
//  함수: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  용도: 주 창의 메시지를 처리합니다.
//
//  WM_COMMAND  - 애플리케이션 메뉴를 처리합니다.
//  WM_PAINT    - 주 창을 그립니다.
//  WM_DESTROY  - 종료 메시지를 게시하고 반환합니다.
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam,
                         LPARAM lParam) {
    switch (message) {
        case WM_CREATE: {
            HWND hButton = CreateWindowEx(
                0,           // Optional window styles
                L"STATIC",    // Window class
                L"Selected IP Address",  // Button text
                WS_TABSTOP | WS_VISIBLE | WS_CHILD,  // Styles
                50, 50,                // x, y position
                300, 100,               // Button width, height
                hWnd,                  // Parent window
                (HMENU)1,              // Button ID
                (HINSTANCE)GetWindowLongPtr(hWnd, GWLP_HINSTANCE),
                NULL  // Pointer not needed
            );
            break;
        }
        case WM_SIZE: {
        
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            break;
        }
        case WM_COMMAND: {
            int wmId = LOWORD(wParam);
            switch (wmId) {
                case IDM_ABOUT:
                    DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd,
                              About);
                    break;
                case ID_MENU_SENDMSG:
                    DialogBox(hInst, MAKEINTRESOURCE(IDD_SENDMSG_DLG), hWnd,
                              SendMsgToChat);
                    break;
                case ID_MENU_SENDFILE:
                    DialogBox(hInst, MAKEINTRESOURCE(IDD_SENDFILE_DLG), hWnd,
                              SendFileToChat);
                    break;
                case ID_MENU_UPTIME:
                    DialogBox(hInst, MAKEINTRESOURCE(IDD_UPTIME_DLG), hWnd,
                              Uptime);
                    break;
                case ID_MENU_DESTIP:
                    DialogBox(hInst, MAKEINTRESOURCE(IDD_DEST_DLG), hWnd,
                              DestinationIP);
                    break;
                case ID_MENU_DOWNLOADFILE:
                    DialogBox(hInst, MAKEINTRESOURCE(IDD_RECVFILE_DLG), hWnd,
                              DownloadFileFn);
                    break;
                case IDM_EXIT:
                    DestroyWindow(hWnd);
                    break;
                default:
                    return DefWindowProc(hWnd, message, wParam, lParam);
            }
        } break;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            // TODO: 여기에 hdc를 사용하는 그리기 코드를 추가합니다...
            EndPaint(hWnd, &ps);
        } break;
        case WM_DESTROY:
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}
