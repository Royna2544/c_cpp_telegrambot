
#define _CRT_SECURE_NO_WARNINGS
#include <array>
#include <cstdint>

#include "../../../src/include/TryParseStr.hpp"
#include "TgBotSocketIntf.h"
#include "UIComponents.h"
#include <optional>
#include <tchar.h>
#include <absl/log/log.h>

namespace {

std::optional<StringLoader::String> OpenFilePicker(HWND hwnd) {
    OPENFILENAME ofn;  // Common dialog box structure
    TCHAR szFile[MAX_PATH_SIZE]{};  // Buffer for file name
    HANDLE hf = nullptr;            // File handle

    // Initialize OPENFILENAME
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = hwnd;
    ofn.lpstrFile = szFile;
    // Set lpstrFile[0] to '\0' so that GetOpenFileName does not use the
    // contents of szFile to initialize itself.
    ofn.lpstrFile[0] = '\0';
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = _T("All\0*.*\0Text\0*.TXT\0");
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = NULL;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = NULL;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    // Display the Open dialog box
    if (GetOpenFileName(&ofn) == TRUE) 
        return ofn.lpstrFile;
    return std::nullopt;
}

}  // namespace


INT_PTR CALLBACK SendFileToChat(HWND hDlg, UINT message, WPARAM wParam,
                               LPARAM lParam) {
    static HWND hChatId;
    static HWND hFileText;
    static HWND hFileButton;

    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
        case WM_INITDIALOG:
            hChatId = GetDlgItem(hDlg, IDC_CHATID);
            hFileButton = GetDlgItem(hDlg, IDC_BROWSE);
            hFileText = GetDlgItem(hDlg, IDC_SEL_FILE);
            SetFocus(hChatId);
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDSEND: {
                    ChatId chatid = 0;
                    std::array<TCHAR, 64 + 1> chatidbuf = {};
                    std::array<char, MAX_PATH_SIZE> pathbuf = {};
                    StringLoader::String errtext;
                    bool fail = false;
                    auto& loader = StringLoader::getInstance();

                    GetDlgItemText(hDlg, IDC_CHATID, chatidbuf.data(),
                                    chatidbuf.size() - 1);

                    if (Length(chatidbuf.data()) == 0) {
                        errtext = loader.getString(IDS_CHATID_EMPTY);
                        fail = true;
                    } else if (!try_parse(chatidbuf.data(), &chatid)) {
                        errtext = loader.getString(IDS_CHATID_NOTINT);
                        fail = true;
                    }
                    if (!fail) {
                        std::string serverReason;
                        GetDlgItemTextA(hDlg, IDC_SEL_FILE, pathbuf.data(), pathbuf.size());
                        fail = !sendFileToChat(
                            chatid, pathbuf.data(),
                            [&serverReason](const GenericAck* data) {
                                serverReason = data->error_msg;
                            });
                        if (fail) {
                            errtext = loader.getString(IDS_CMD_FAILED_SVR) + kLineBreak;
                            errtext += loader.getString(IDS_CMD_FAILED_SVR_RSN);
                            errtext += StringLoader::String(serverReason.begin(), serverReason.end());
                        } else {
                            MessageBox(
                                hDlg,
                                loader.getString(IDS_SUCCESS_FILESENT).c_str(),
                                loader.getString(IDS_SUCCESS).c_str(),
                                        MB_ICONINFORMATION | MB_OK);
                        }
                    }
                    if (fail) {
                        MessageBox(hDlg, errtext.c_str(),
                                   loader.getString(IDS_FAILED).c_str(),
                                    MB_ICONERROR | MB_OK);
                    }
                    return DIALOG_OK;
                }
                case IDC_BROWSE:
                    if (const auto f = OpenFilePicker(hDlg); f) {
                        SetDlgItemText(hDlg, IDC_SEL_FILE, f->c_str());
                    }
                    return DIALOG_OK;

                case IDCANCEL:
                    EndDialog(hDlg, LOWORD(wParam));
                    return DIALOG_OK;
                default:
                    break;
            };
            break;
    }
    return DIALOG_NO;
}
