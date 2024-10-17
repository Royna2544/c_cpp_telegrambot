#include <fmt/format.h>

#include <sstream>
#include <string>

namespace progressbar {

constexpr int MAX = 100;
constexpr std::string_view FILLED = "■";
constexpr std::string_view EMPTY = "□";
constexpr int DIVIDER = 8;
constexpr int TOTAL_BARS = MAX / DIVIDER;
constexpr int MIDDLE_INDEX = TOTAL_BARS / 2;

inline std::string create(double progress) {
    const auto filledBars = static_cast<int>(progress / DIVIDER);

    std::ostringstream colorBar;
    int index = 0;
    colorBar << "[";

    while (index < TOTAL_BARS) {
        // Insert percentage string at the middle and skip the next few bars
        if (index == MIDDLE_INDEX) {
            colorBar << fmt::format(" {:.2f}% ", progress);
        }
        if (index < filledBars) {
            colorBar << FILLED;
        } else if (index < TOTAL_BARS) {
            colorBar << EMPTY;
        }
        ++index;
    }

    colorBar << "]";

    return colorBar.str();
}
}  // namespace progressbar