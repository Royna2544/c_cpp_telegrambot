#define _CRT_SECURE_NO_WARNINGS

#include "BlockingOperation.h"

#include <WinUser.h>

#include "UIComponents.h"

bool _BlockingOperation::shouldBlockRequest(HWND hDlg) const {
    if (m_enabled) {
        auto &loader = StringLoader::getInstance();
        MessageBox(hDlg, loader.getString(IDS_OPERATION_ACTIVE).c_str(),
                   loader.getString(IDS_FAILED).c_str(), WARNING_DIALOG);
        return true;
    }
    return false;
}

void _BlockingOperation::start() {
    SetCursor(LoadCursor(NULL, IDC_WAIT));
    m_enabled = true;
}

void _BlockingOperation::stop() {
    SetCursor(LoadCursor(NULL, IDC_ARROW));
    m_enabled = false;
}
