
#define _CRT_SECURE_NO_WARNINGS
#include <absl/log/log.h>
#include <process.h>
#include <tchar.h>

#include <array>
#include <map>
#include <optional>

#include "../../../src/include/TryParseStr.hpp"
#include "BlockingOperation.h"
#include "TgBotSocketIntf.h"
#include "UIComponents.h"

namespace {

struct DownloadFileInput {
    char srcFilePath[MAX_PATH_SIZE];
    char destFilePath[MAX_PATH_SIZE];
    HWND dialog;
};

std::optional<StringLoader::String> OpenFilePicker(HWND hwnd) {
    OPENFILENAME ofn;               // Common dialog box structure
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
    ofn.Flags = 0;

    // Display the Open dialog box
    if (GetOpenFileName(&ofn) == TRUE) return ofn.lpstrFile;
    return std::nullopt;
}

unsigned __stdcall SendFileTask(void* param) {
    const auto* in = (DownloadFileInput*)param;
    GenericAck* result = nullptr;

    result = allocMem<GenericAck>();
    if (result == nullptr) {
        PostMessage(in->dialog, WM_DLFILE_RESULT, 0, (LPARAM) nullptr);
        free(param);
        return 0;
    }
    downloadFile(in->destFilePath, in->srcFilePath,
                 [result](const GenericAck* ack) { *result = *ack; });

    PostMessage(in->dialog, WM_DLFILE_RESULT, 0, (LPARAM)result);
    free(param);
    return 0;
}

}  // namespace

INT_PTR CALLBACK DownloadFileFn(HWND hDlg, UINT message, WPARAM wParam,
                              LPARAM lParam) {
    static HWND hRemoteFilepath;
    static HWND hFileText;
    static HWND hFileButton;
    static BlockingOperation blk;

    UNREFERENCED_PARAMETER(lParam);

    switch (message) {
        case WM_INITDIALOG:
            hRemoteFilepath = GetDlgItem(hDlg, IDC_REMOTE_FILEPATH);
            hFileButton = GetDlgItem(hDlg, IDC_BROWSE);
            hFileText = GetDlgItem(hDlg, IDC_SEL_FILE);

            // Focus to only one input section
            SetFocus(hRemoteFilepath);
            return DIALOG_OK;

        case WM_COMMAND:
            if (blk.shouldBlockRequest(hDlg)) {
                return DIALOG_OK;
            }

            switch (LOWORD(wParam)) {
                case IDSEND: {
                    std::array<char, MAX_PATH_SIZE> srcbuf = {};
                    std::array<char, MAX_PATH_SIZE> destbuf = {};
                    auto& loader = StringLoader::getInstance();

                    GetDlgItemTextA(hDlg, IDC_REMOTE_FILEPATH, srcbuf.data(),
                                    srcbuf.size() - 1);
                    GetDlgItemTextA(hDlg, IDC_SEL_FILE, destbuf.data(),
                                   destbuf.size());

                    auto* in = allocMem<DownloadFileInput>();

                    if (in) {
                        blk.start();
                        in->dialog = hDlg;
                        strncpy(in->destFilePath, destbuf.data(),
                                MAX_PATH_SIZE - 1);
                        in->destFilePath[MAX_PATH_SIZE - 1] = 0;
                        strncpy(in->srcFilePath, srcbuf.data(),
                                MAX_PATH_SIZE - 1);
                        in->srcFilePath[MAX_PATH_SIZE - 1] = 0;
                        _beginthreadex(NULL, 0, SendFileTask, in, 0, NULL);
                    } else {
                        MessageBox(hDlg, _T("Failed to allocate memory"),
                                   loader.getString(IDS_FAILED).c_str(),
                                   ERROR_DIALOG);
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
        case WM_SENDFILE_RESULT: {
            if (lParam == NULL) {
                blk.stop();
                return DIALOG_OK;
            }
            const auto* res = reinterpret_cast<GenericAck*>(lParam);
            auto& loader = StringLoader::getInstance();

            if (res->result != AckType::SUCCESS) {
                StringLoader::String errtext;
                errtext = loader.getString(IDS_CMD_FAILED_SVR) + kLineBreak;
                errtext += loader.getString(IDS_CMD_FAILED_SVR_RSN);
                errtext += charToWstring(res->error_msg.data());
                MessageBox(hDlg, errtext.c_str(),
                           loader.getString(IDS_FAILED).c_str(), ERROR_DIALOG);
            } else {
                MessageBox(hDlg, loader.getString(IDS_SUCCESS_FILESENT).c_str(),
                           loader.getString(IDS_SUCCESS).c_str(), INFO_DIALOG);
            }
            blk.stop();
            free(reinterpret_cast<void*>(lParam));
            return DIALOG_OK;
        }
    }
    return DIALOG_NO;
}
