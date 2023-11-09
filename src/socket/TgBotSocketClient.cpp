#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <sstream>
#include <stdexcept>

#include "TgBotSocket.h"

static void usage(char* argv, bool success) {
    printf("Usage: %s [cmd enum value] [args...]\n", argv);
    exit(success);
    __builtin_unreachable();
}

static std::map<TgBotCommand, int> kRequiredArgsCount = {
    {CMD_WRITE_MSG_TO_CHAT_ID, 2},  // chatid, msg
    {CMD_CTRL_SPAMBLOCK, 1},        // Policy
};

static bool verifyArgsCount(TgBotCommand cmd, int argc) {
    if (kRequiredArgsCount.find(cmd) == kRequiredArgsCount.end())
        throw std::runtime_error("Cannot find cmd in argcount map!");
    auto it = kRequiredArgsCount[cmd];
    bool ret = it == argc;
    if (!ret)
        fprintf(stderr, "Invalid argument count %d for cmd %d, %d required\n", argc, cmd, it);
    return ret;
}

static bool stoi_or(std::string str, int32_t* intval) {
    try {
        *intval = std::stoi(str.c_str());
    } catch (...) {
        fprintf(stderr, "Failed to parse '%s' to int\n", str.c_str());
        return false;
    }
    return true;
}

static bool stol_or(std::string str, int64_t* intval) {
    try {
        *intval = std::stol(str.c_str());
    } catch (...) {
        fprintf(stderr, "Failed to parse '%s' to long\n", str.c_str());
        return false;
    }
    return true;
}

template <class C>
bool verifyWithinEnum(C max, int val) { return val >= 0 && val < max; }

int main(int argc, char** argv) {
    int cmd_i;
    enum TgBotCommand cmd;
    union TgBotCommandUnion data_g;
    char* exe = argv[0];

    if (argc == 1)
        usage(exe, true);

    // Remove exe (argv[0])
    ++argv;
    --argc;

    if (!stoi_or(*argv, &cmd_i)) {
        fprintf(stderr, "Cannot parse cmd enum value from '%s'", argv[1]);
        goto error;
    }
    if (!verifyWithinEnum(CMD_MAX, cmd_i) || cmd_i == CMD_EXIT) {
        fprintf(stderr, "Invalid cmd enum value: %d\n", cmd_i);
        goto error;
    }

    // Remove cmd (argv[1])
    ++argv;
    --argc;

    cmd = static_cast<decltype(cmd)>(cmd_i);
    if (!verifyArgsCount(cmd, argc))
        goto error;
    switch (cmd) {
        case CMD_WRITE_MSG_TO_CHAT_ID: {
            TgBotCommandData::WriteMsgToChatId data;
            if (!stol_or(argv[0], &data.to)) {
                goto error;
            }
            memset(data.msg, 0, sizeof(data.msg));
            strncpy(data.msg, argv[1], strlen(argv[3]));
            data_g.data_1 = data;
            break;
        }
        case CMD_CTRL_SPAMBLOCK: {
            int tmp;
            if (stoi_or(argv[0], &tmp) && verifyWithinEnum(TgBotCommandData::CTRL_MAX, tmp)) {
                data_g.data_2 = static_cast<decltype(data_g.data_2)>(tmp);
            } else {
		fprintf(stderr, "Failed to convert '%s' to ctrl enum value\n", argv[0]);
                goto error;
	    }
            break;
        }
        case CMD_EXIT:
        case CMD_MAX:
            goto error;
        default:
            throw std::runtime_error("Unhandled command value!");
    };
    writeToSocket({cmd, data_g});
    return EXIT_SUCCESS;
error:
    usage(exe, false);
    return EXIT_FAILURE;
}
