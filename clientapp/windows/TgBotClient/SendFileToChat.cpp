
#define _CRT_SECURE_NO_WARNINGS
#include <absl/log/log.h>
#include <process.h>
#include <tchar.h>

#include <map>
#include <array>
#include <optional>

#include "../../../src/include/TryParseStr.hpp"
#include "BlockingOperation.h"
#include "TgBotSocketIntf.h"
#include "UIComponents.h"

namespace {

struct SendFileInput {
    ChatId chatid;
    char filePath[MAX_PATH_SIZE];
    FileType fileType;
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
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    // Display the Open dialog box
    if (GetOpenFileName(&ofn) == TRUE) return ofn.lpstrFile;
    return std::nullopt;
}

unsigned __stdcall SendFileTask(void* param) {
    const auto* in = (SendFileInput*)param;
    GenericAck* result = nullptr;

    result = allocMem<GenericAck>();
    if (result == nullptr) {
        PostMessage(in->dialog, WM_SENDFILE_RESULT, 0, (LPARAM) nullptr);
        free(param);
        return 0;
    }
    sendFileToChat(in->chatid, in->filePath, in->fileType, 
        [result](const GenericAck* data) { 
         *result = *data;
    });

    PostMessage(in->dialog, WM_SENDFILE_RESULT, 0, (LPARAM)result);
    free(param);
    return 0;
}

void updateToType(std::map<FileType, HWND>& ref_in, FileType& type, FileType toUpdate) { 
    if (type != toUpdate) {
        SendMessage(ref_in[type], BM_SETCHECK, BST_UNCHECKED, 0);
        SendMessage(ref_in[toUpdate], BM_SETCHECK, BST_CHECKED, 0);
        type = toUpdate;
    }
}
}  // namespace

INT_PTR CALLBACK SendFileToChat(HWND hDlg, UINT message, WPARAM wParam,
                                LPARAM lParam) {
    static HWND hChatId;
    static HWND hFileText;
    static HWND hFileButton;
    static HWND hFileAsDoc;
    static HWND hFileAsPic;
    static BlockingOperation blk;
    static FileType type;
    static std::map<FileType, HWND> refs = {
        {FileType::TYPE_DOCUMENT, hFileAsDoc},
        {FileType::TYPE_PHOTO, hFileAsDoc}
    };

    UNREFERENCED_PARAMETER(lParam);

    switch (message) {
        case WM_INITDIALOG:
            hChatId = GetDlgItem(hDlg, IDC_CHATID);
            hFileButton = GetDlgItem(hDlg, IDC_BROWSE);
            hFileText = GetDlgItem(hDlg, IDC_SEL_FILE);
            hFileAsDoc = GetDlgItem(hDlg, IDC_ASFILE);
            hFileAsPic = GetDlgItem(hDlg, IDC_ASPHOTO);

            // Default type: Document
            SendMessage(hFileAsDoc, BM_SETCHECK, BST_CHECKED, 0);
            type = FileType::TYPE_DOCUMENT;

            // Focus to only one input section
            SetFocus(hChatId);

            blk.init();
            return DIALOG_OK;

        case WM_COMMAND:
            if (blk.shouldBlockRequest(hDlg)) {
                return DIALOG_OK;
            }

            switch (LOWORD(wParam)) {
                case IDSEND: {
                    ChatId chatid = 0;
                    std::array<TCHAR, 64 + 1> chatidbuf = {};
                    std::array<char, MAX_PATH_SIZE> pathbuf = {};
                    std::optional<StringLoader::String> errtext;
                    bool fail = false;
                    auto& loader = StringLoader::getInstance();

                    GetDlgItemText(hDlg, IDC_CHATID, chatidbuf.data(),
                                   chatidbuf.size() - 1);
                    GetDlgItemTextA(hDlg, IDC_SEL_FILE, pathbuf.data(),
                                    pathbuf.size());

                    if (Length(chatidbuf.data()) == 0) {
                        errtext = loader.getString(IDS_CHATID_EMPTY);
                    } else if (!try_parse(chatidbuf.data(), &chatid)) {
                        errtext = loader.getString(IDS_CHATID_NOTINT);
                    }
                    if (!errtext.has_value()) {
                        auto* in = allocMem<SendFileInput>();

                        if (in) {
                            blk.start();

                            in->chatid = chatid;
                            in->dialog = hDlg;
                            strncpy(in->filePath, pathbuf.data(),
                                    MAX_PATH_SIZE - 1);
                            in->filePath[MAX_PATH_SIZE - 1] = 0;
                            in->fileType = type;
                            _beginthreadex(NULL, 0, SendFileTask, in, 0, NULL);
                        } else {
                            errtext = _T("Failed to allocate memory");
                        }
                    }
                    if (errtext) {
                        MessageBox(hDlg, errtext->c_str(),
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
                case IDC_ASFILE:
                    updateToType(refs, type, FileType::TYPE_DOCUMENT);
                    break;
                case IDC_ASPHOTO:
                    updateToType(refs, type, FileType::TYPE_PHOTO);
                    break;
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
                           loader.getString(IDS_FAILED).c_str(),
                           ERROR_DIALOG);
            } else {
                MessageBox(hDlg, loader.getString(IDS_SUCCESS_FILESENT).c_str(),
                           loader.getString(IDS_SUCCESS).c_str(),
                           INFO_DIALOG);
            }
            blk.stop();
            free(reinterpret_cast<void*>(lParam));
            return DIALOG_OK;
        }
    }
    return DIALOG_NO;
}
