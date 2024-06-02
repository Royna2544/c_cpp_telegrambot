#pragma once

#include <wtypes.h>

class BlockingOperation {
   public:
    bool shouldBlockRequest(HWND hDlg) const;
    void start();
    void stop();

   private:
    bool m_enabled;
};
