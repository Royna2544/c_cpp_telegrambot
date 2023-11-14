#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <vector>

#include "../include/RuntimeException.h"
#include "TgBotSocket.h"

static void usage(const char* argv, bool success) {
    using sortedIt = decltype(kTgBotCommandStrMap)::value_type;

    printf("Usage: %s [cmd enum value] [args...]\n\n", argv);
    printf("Available cmd enum values:\n");
    auto kSortedStrMap = kTgBotCommandStrMap;
    std::sort(kSortedStrMap.begin(), kSortedStrMap.end(), [](const sortedIt& v1, const sortedIt& v2) {
        return v1.second > v2.second;
    });
    for (const auto& ent : kSortedStrMap) {
        if (ent.first == CMD_EXIT || ent.first == CMD_MAX) continue;
        printf("%s: value %d, Requires %d argument(s)\n", ent.second.c_str(),
               ent.first, toCount(ent.first));
    }
    exit(!success);
    __builtin_unreachable();
}

static bool verifyArgsCount(TgBotCommand cmd, int argc) {
    int required = toCount(cmd);
    if (required != argc) {
        fprintf(stderr, "Invalid argument count %d for cmd %s, %d required\n", argc,
                toStr(cmd).c_str(), required);
        return false;
    }
    return true;
}

static bool stoi_or(const std::string& str, int32_t* intval) {
    try {
        *intval = std::stoi(str);
    } catch (...) {
        fprintf(stderr, "Failed to parse '%s' to int\n", str.c_str());
        return false;
    }
    return true;
}

static bool stol_or(const std::string& str, int64_t* intval) {
    try {
        *intval = std::stol(str);
    } catch (...) {
        fprintf(stderr, "Failed to parse '%s' to long\n", str.c_str());
        return false;
    }
    return true;
}

static bool stob_or(const std::string& str, bool* val) {
    int intvar = 0;
    bool rc = stoi_or(str, &intvar);
    if (rc)
        *val = !!intvar;
    return rc;
}

template <class C>
bool verifyWithinEnum(C max, int val) { return val >= 0 && val < max; }

template <class C>
bool parseOneEnum(C* res, C max, const char* str, const char* name) {
    int parsed = 0;
    if (stoi_or(str, &parsed)) {
        if (verifyWithinEnum(max, parsed)) {
            *res = static_cast<C>(parsed);
            return true;
        } else {
            fprintf(stderr, "Cannot convert %s to %s enum value\n", str, name);
        }
    }
    return false;
}

int main(int argc, char** argv) {
    enum TgBotCommand cmd = CMD_MAX;
    union TgBotCommandUnion data_g {};
    const char* exe = argv[0];
    bool ret = false;

    if (argc == 1)
        usage(exe, true);

    // Remove exe (argv[0])
    ++argv;
    --argc;

    if (parseOneEnum(&cmd, CMD_MAX, *argv, "cmd")) {
        if (cmd == CMD_EXIT) {
            fprintf(stderr, "CMD_EXIT is not supported\n");
        } else {
            // Remove cmd (argv[1])
            ++argv;
            --argc;

            if (verifyArgsCount(cmd, argc)) {
                switch (cmd) {
                    case CMD_WRITE_MSG_TO_CHAT_ID: {
                        TgBotCommandData::WriteMsgToChatId data;
                        if (!stol_or(argv[0], &data.to)) {
                            break;
                        }
                        memset(data.msg, 0, sizeof(data.msg));
                        strncpy(data.msg, argv[1], sizeof(data.msg));
                        data_g.data_1 = data;
                        ret = true;
                        break;
                    }
                    case CMD_CTRL_SPAMBLOCK: {
                        ret = parseOneEnum(&data_g.data_3, TgBotCommandData::CTRL_MAX,
                                           argv[0], "spamblock");
                        break;
                    }
                    case CMD_OBSERVE_CHAT_ID: {
                        TgBotCommandData::ObserveChatId data;
                        ret = stol_or(argv[0], &data.id) && stob_or(argv[1], &data.observe);
                        data_g.data_4 = data;
                    } break;
                    case CMD_MAX:
                        break;
                    default:
                        throw runtime_errorf("Unhandled command value: %d!", cmd);
                };
            }
        }
    }
    if (ret)
        writeToSocket({cmd, data_g});
    else
        usage(exe, false);
    return !ret;
}
