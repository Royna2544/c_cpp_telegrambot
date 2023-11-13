#pragma once

#include <ostream>
#include <ctime>

union time {
    std::time_t val;
};

inline std::ostream &operator<<(std::ostream &self, union time t) {
    char timestr[std::size("yyyy-mm-ddThh:mm:ssZ")];
    std::strftime(std::data(timestr), std::size(timestr),
                  "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&t.val));
    return self << timestr;
}

inline int operator-(union time thisone, union time otherone) {
    return thisone.val - otherone.val;
}
