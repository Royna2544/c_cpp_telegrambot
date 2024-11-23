
#define _CRT_SECURE_NO_WARNINGS
#include "TgBotSocketIntf.h"
#include "UIComponents.h"
#include <CommCtrl.h>

INT_PTR CALLBACK DestinationIP(HWND hDlg, UINT message, WPARAM wParam,
                               LPARAM lParam) {
    static HWND hIPAddr;
    static HWND hUseINet4;
    static HWND hUseINet6;
    static std::optional<SocketConfig> config;

    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
        case WM_INITDIALOG:
            hIPAddr = GetDlgItem(hDlg, IDC_IPADDR);
            hUseINet4 = GetDlgItem(hDlg, IDC_INET_4);
            hUseINet6 = GetDlgItem(hDlg, IDC_INET_6);
            SendMessage(hUseINet4, BM_SETCHECK, BST_CHECKED, 0);
            config.emplace();
            config->mode = SocketConfig::Mode::USE_IPV4;
            config->port = 50000;
            return DIALOG_OK;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_INET_4:
                    config->mode = SocketConfig::Mode::USE_IPV4;
                    break;
                case IDC_INET_6:
                    config->mode = SocketConfig::Mode::USE_IPV6;
                    break;
                case IDC_IPADDR: {
                    DWORD dwAddr;
                    SendMessage(hIPAddr, IPM_GETADDRESS, 0, (LPARAM)&dwAddr);

                    // Extract the individual parts of the IP address
                    BYTE b1 = FIRST_IPADDRESS(dwAddr);
                    BYTE b2 = SECOND_IPADDRESS(dwAddr);
                    BYTE b3 = THIRD_IPADDRESS(dwAddr);
                    BYTE b4 = FOURTH_IPADDRESS(dwAddr);

                    CHAR szIpAddress[16];
                    sprintf(szIpAddress, "%d.%d.%d.%d", b1, b2, b3, b4);
                    config->address = szIpAddress;
                    break;
                }
                case IDOK: {
                    auto &loader = StringLoader::getInstance();
                    if (config->address.empty()) {
                        MessageBox(hDlg,
                                   loader.getString(IDS_IPADDR_EMPTY).c_str(),
                                   loader.getString(IDS_FAILED).c_str(),
                                   MB_ICONWARNING | MB_OK);
                    } else {
                        setSocketConfig(config.value());
                        config.reset();
                        EndDialog(hDlg, LOWORD(wParam));
                    }
                    break;
                }
                case IDCANCEL:
                    config.reset();
                    EndDialog(hDlg, LOWORD(wParam));
                    break;
                default:
                    return DIALOG_NO;
            };
            return DIALOG_OK;
        default:
            break;
    }
    return DIALOG_NO;
}
