#include <fmt/format.h>

#include <AbslLogInit.hpp>

#include "RepoUtils.hpp"

using std::string_view_literals::operator""sv;

int main(int argc, const char** argv) {
    TgBot_AbslLogInit();
    if (argc != 5) {
        fmt::print("Usage: {} <gitDir> <url> <branch> <op>\n", argv[0]);
        return EXIT_FAILURE;
    }
    GitBranchSwitcher bs(argv[1]);

    if (!bs.open()) {
        fmt::print("Cannot open repo\n");
        return EXIT_FAILURE;
    }

    RepoInfo info{argv[2], argv[3]};

    fmt::print(R"(GitBranchSwitcherTest
GitDirectory: {}
URL: {}
Branch: {}
Op: {}
)",
               argv[1], argv[2], argv[3], argv[4]);
    bool ret;
    auto op(argv[4]);
    if (op == "check"sv) {
        ret = bs.check(info);
    } else if (op == "checkout"sv) {
        ret = bs.checkout(info);
    } else {
        fmt::print("Invalid op: {}\n", op);
        return EXIT_FAILURE;
    }
    fmt::print("Result: {}\n", ret);
}
