#include <random>

#include <../utils/LinuxPort.h>
#include <RuntimeException.h>

int genRandomNumber(const int min, const int max) {
    std::random_device rd;
    std::mt19937 gen(rd());

    WARN_ONCE(rd.entropy() == 0, "Pesudo RNG detected");
    if (min >= max) {
        throw runtime_errorf("min(%d) >= max(%d)", min, max);
    }
    std::uniform_int_distribution<int> distribution(min, max);
    return distribution(gen);
}

int genRandomNumber(const int max) {
    return genRandomNumber(0, max);
}
