#define _CRT_SECURE_NO_WARNINGS
#include "TgBotSocketIntf.h"
#include "UIComponents.h"

INT_PTR CALLBACK Uptime(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    static HWND hUptimeText;

    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
        case WM_INITDIALOG:
            hUptimeText = GetDlgItem(hDlg, IDC_UPTIME);
            return DIALOG_OK;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_LOAD:
                    SetDlgItemTextA(hDlg, IDC_UPTIME, getUptime().c_str());
                    return DIALOG_OK;
                case IDCANCEL:
                case IDOK:
                    EndDialog(hDlg, LOWORD(wParam));
                    return DIALOG_OK;
                default:
                    break;
            };
            break;
    }
    return DIALOG_NO;
}