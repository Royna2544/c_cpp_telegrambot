#include "Selectors.hpp"

struct SelectSelector : Selector {
    bool init() override;
    bool add(socket_handle_t fd, OnSelectedCallback callback, Mode mode) override;
    bool remove(socket_handle_t fd) override;
    SelectorPollResult poll() override;
    void shutdown() override;
    bool reinit() override;
    bool isTimeoutAvailable() const override { return false; }

   private:
    struct SelectFdData {
        socket_handle_t fd;
        OnSelectedCallback callback;
        Mode mode;
    };
    fd_set read_set;
    fd_set write_set;
    std::vector<SelectFdData> data;
};
