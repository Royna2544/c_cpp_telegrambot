
#define _CRT_SECURE_NO_WARNINGS
#include <array>

#include "../../../src/include/TryParseStr.hpp"
#include "TgBotSocketIntf.h"
#include "UIComponents.h"
#include "BlockingOperation.h"
#include <process.h>

namespace {

struct SendMsgInput {
    ChatId chatid;
    char message[MAX_PATH_SIZE];
    HWND dialog;
};
struct SendMsgResult {
    bool success;
    char reason[MAX_MSG_SIZE];
};

unsigned __stdcall SendMsgTask(void* param) {
    const auto* in = (SendMsgInput*)param;
    SendMsgResult* result = nullptr;

    result = allocMem<SendMsgResult>();
    if (result == nullptr) {
        PostMessage(in->dialog, WM_SENDMSG_RESULT, 0, (LPARAM) nullptr);
        free(param);
        return 0;
    }
    sendMessageToChat(in->chatid, in->message, [result](const GenericAck* data) {
        result->success = data->result == AckType::SUCCESS;
        if (!result->success) {
            copyTo(result->reason, data->error_msg, MAX_MSG_SIZE);
        }
    });

    PostMessage(in->dialog, WM_SENDMSG_RESULT, 0, (LPARAM)result);
    free(param);
    return 0;
}
}  // namespace

INT_PTR CALLBACK SendMsgToChat(HWND hDlg, UINT message, WPARAM wParam,
                               LPARAM lParam) {
    static HWND hChatId;
    static HWND hMsgText;
    static BlockingOperation blk;

    UNREFERENCED_PARAMETER(lParam);
    switch (message) {
        case WM_INITDIALOG:
            hChatId = GetDlgItem(hDlg, IDC_CHATID);
            hMsgText = GetDlgItem(hDlg, IDC_MESSAGETXT);
            blk.init();
            return DIALOG_OK;

        case WM_COMMAND:
            if (blk.shouldBlockRequest(hDlg)) {
                return DIALOG_OK;
            }
            switch (LOWORD(wParam)) {
                case IDSEND: {
                    ChatId chatid = 0;
                    std::array<char, MAX_MSG_SIZE> msgbuf = {};
                    std::array<TCHAR, 64 + 1> chatidbuf = {};
                    std::optional<StringLoader::String> errtext;
                    auto& loader = StringLoader::getInstance();

                    GetDlgItemText(hDlg, IDC_CHATID, chatidbuf.data(),
                                   chatidbuf.size());
                    GetDlgItemTextA(hDlg, IDC_MESSAGETXT, msgbuf.data(),
                                    msgbuf.size());
                    if (Length(chatidbuf.data()) == 0) {
                        errtext = loader.getString(IDS_CHATID_EMPTY);
                    } else if (Length(msgbuf.data()) == 0) {
                        errtext = loader.getString(IDS_MSG_EMPTY);
                    } else if (!try_parse(chatidbuf.data(), &chatid)) {
                        errtext = loader.getString(IDS_CHATID_NOTINT);
                    }
                    if (!errtext) {
                        auto* in = allocMem<SendMsgInput>();

                        if (in) {
                            blk.start();
                            in->chatid = chatid;
                            in->dialog = hDlg;
                            strncpy(in->message, msgbuf.data(),
                                    MAX_MSG_SIZE - 1);
                            in->message[MAX_MSG_SIZE - 1] = 0;
                            _beginthreadex(NULL, 0, SendMsgTask, in, 0, NULL);
                        } else {
                            errtext = _T("Failed to allocate memory");
                        }
                    }
                    return DIALOG_OK;
                }

                case IDCANCEL:
                    EndDialog(hDlg, LOWORD(wParam));
                    return DIALOG_OK;
            };
            break;
        case WM_SENDMSG_RESULT: {
            if (lParam == NULL) {
                return DIALOG_OK;
            }
            const auto* res = reinterpret_cast<SendMsgResult*>(lParam);
            auto& loader = StringLoader::getInstance();

            if (!res->success) {
                StringLoader::String errtext;
                errtext = loader.getString(IDS_CMD_FAILED_SVR) + kLineBreak;
                errtext += loader.getString(IDS_CMD_FAILED_SVR_RSN);
                errtext += charToWstring(res->reason);
                MessageBox(hDlg, errtext.c_str(),
                           loader.getString(IDS_FAILED).c_str(),
                           ERROR_DIALOG);
            } else {
                MessageBox(hDlg, loader.getString(IDS_SUCCESS_MSGSENT).c_str(),
                           loader.getString(IDS_SUCCESS).c_str(),
                           INFO_DIALOG);
            }
            blk.stop();
            free(reinterpret_cast<void*>(lParam));
            return DIALOG_OK;
        }
        default:
            break;
    }
    return DIALOG_NO;
}
