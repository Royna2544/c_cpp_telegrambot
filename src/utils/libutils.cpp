#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <random>

#define ARRAY_SIZE(arr) sizeof(arr) / sizeof(arr[0])

void findCompiler(char** c, char** cxx) {
    static const char* const compilers[][2] = {
        {"clang", "clang++"},
        {"gcc", "g++"},
        {"cc", "c++"},
    };
    static char buffer[20];
    for (int i = 0; i < ARRAY_SIZE(compilers); i++) {
        auto checkfn = [i](const int idx) -> bool {
            memset(buffer, 0, sizeof(buffer));
            auto bytes = snprintf(buffer, sizeof(buffer), "/usr/bin/%s",
                                  compilers[i][idx]);
            if (bytes >= sizeof(buffer))
                return false;
            else
                buffer[bytes] = '\0';
            return access(buffer, R_OK | X_OK) == 0;
        };
        if (!*c && checkfn(0)) {
            *c = strdup(buffer);
        }
        if (!*cxx && checkfn(1)) {
            *cxx = strdup(buffer);
        }
        if (*c && *cxx) break;
    }
}

int genRandomNumber(const int num1, const int num2) {
    std::random_device rd;
    std::mt19937 gen(rd());
    int num1_l = num1, num2_l = num2;
    if (num1 > num2) {
        num1_l = num2;
        num2_l = num1;
    }
    std::uniform_int_distribution<int> distribution(num1_l, num2_l);
    return distribution(gen);
}
