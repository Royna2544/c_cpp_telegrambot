#pragma once

#include <Windows.h>
#include <optional>

class _BlockingOperation {
   public:
    bool shouldBlockRequest(HWND hDlg) const;
    void start();
    void stop();

   private:
    bool m_enabled;
};

struct BlockingOperation {
    std::optional<_BlockingOperation> _opt;

    void init() {
        if (_opt) {
            _opt.reset();
        }
    }

    bool shouldBlockRequest(HWND hDlg) const {
        if (_opt) {
            return _opt->shouldBlockRequest(hDlg);
        }
        return false;
    }

    void start() {
        if (!_opt) {
            _opt.emplace();
        }
        _opt->start();
    }
    void stop() {
        if (_opt) {
            _opt->stop();
        }
    }
};