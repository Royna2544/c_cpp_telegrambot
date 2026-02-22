#include <fmt/format.h>

#include <TryParseStr.hpp>
#include <cstdlib>

#include "Progress.hpp"

int app_main(int argc, char* argv[]) {
    // Check if the user provided a percentage argument
    if (argc != 2) {
        fmt::print(stderr, "Usage: {} <percentage>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // Convert the input argument to a double
    double percent = 0;

    if (!try_parse(argv[1], &percent) || (percent < 0.0 && percent > 100.0)) {
        (void)fputs("Error: Invalid percentage format.", stderr);
        return EXIT_FAILURE;
    }

    // Call the create function
    fmt::print("Progress bar: {}\n", progressbar::create(percent));
    return EXIT_SUCCESS;
}