#include "Selectors.hpp"

struct SelectSelector : Selector {
    bool init() override;
    bool add(socket_handle_t fd, OnSelectedCallback callback) override;
    bool remove(socket_handle_t fd) override;
    SelectorPollResult poll() override;
    void shutdown() override;
    bool reinit() override;
    bool isTimeoutAvailable() const override { return false; }

   private:
    struct SelectFdData {
        socket_handle_t fd;
        OnSelectedCallback callback;
    };
    fd_set set;
    std::vector<SelectFdData> data;
};
