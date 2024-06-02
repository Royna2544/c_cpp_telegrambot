
#define _CRT_SECURE_NO_WARNINGS
#include <array>

#include "../../../src/include/TryParseStr.hpp"
#include "TgBotSocketIntf.h"
#include "UIComponents.h"

INT_PTR CALLBACK SendMsgToChat(HWND hDlg, UINT message, WPARAM wParam,
                               LPARAM lParam) {
    static HWND hChatId;
    static HWND hMsgText;

    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
        case WM_INITDIALOG:
            hChatId = GetDlgItem(hDlg, IDC_CHATID);
            hMsgText = GetDlgItem(hDlg, IDC_MESSAGETXT);
            return (INT_PTR)TRUE;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDSEND: {
                    ChatId chatid = 0;
                    std::array<char, MAX_MSG_SIZE> msgbuf = {};
                    std::array<TCHAR, 64 + 1> chatidbuf = {};
                    StringLoader::String errtext;
                    bool fail = false;
                    auto& loader = StringLoader::getInstance();

                    GetDlgItemText(hDlg, IDC_CHATID, chatidbuf.data(),
                                   chatidbuf.size());
                    GetDlgItemTextA(hDlg, IDC_MESSAGETXT, msgbuf.data(),
                                    msgbuf.size());
                    if (Length(chatidbuf.data()) == 0) {
                        errtext = loader.getString(IDS_CHATID_EMPTY);
                        fail = true;
                    } else if (Length(msgbuf.data()) == 0) {
                        errtext = loader.getString(IDS_MSG_EMPTY);
                        fail = true;
                    } else if (!try_parse(chatidbuf.data(), &chatid)) {
                        errtext = loader.getString(IDS_CHATID_NOTINT);
                        fail = true;
                    }
                    if (!fail) {
                        std::string serverReason;
                        fail = !sendMessageToChat(
                            chatid, msgbuf.data(),
                            [&serverReason](const GenericAck* data) {
                                serverReason = data->error_msg;
                            });
                        if (fail) {
                            errtext = loader.getString(IDS_CMD_FAILED_SVR) +
                                      kLineBreak;
                            errtext += loader.getString(IDS_CMD_FAILED_SVR_RSN);
                            errtext += StringLoader::String(
                                serverReason.begin(), serverReason.end());
                        } else {
                            MessageBox(
                                hDlg,
                                loader.getString(IDS_SUCCESS_MSGSENT).c_str(),
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

                case IDCANCEL:
                    EndDialog(hDlg, LOWORD(wParam));
                    return DIALOG_OK;
            };
            break;
        default:
            break;
    }
    return DIALOG_NO;
}
