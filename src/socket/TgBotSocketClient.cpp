#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

#include "../include/RuntimeException.h"
#include "TgBotSocket.h"

static void usage(const char* argv, bool success) {
    std::cout << "Usage: " << argv << " [cmd enum value] [args...]" << std::endl
              << std::endl;
    std::cout << "Available cmd enum values:" << std::endl;
    std::cout << TgBotCmd_getHelpText();

    exit(!success);
    __builtin_unreachable();
}

static bool verifyArgsCount(TgBotCommand cmd, int argc) {
    int required = TgBotCmd_toCount(cmd);
    if (required != argc) {
        fprintf(stderr, "Invalid argument count %d for cmd %s, %d required\n", argc,
                TgBotCmd_toStr(cmd).c_str(), required);
        return false;
    }
    return true;
}

static bool stoi_or(const std::string& str, std::int32_t* intval) {
    try {
        *intval = std::stoi(str);
    } catch (...) {
        fprintf(stderr, "Failed to parse '%s' to int32_t\n", str.c_str());
        return false;
    }
    return true;
}

static bool stol_or(const std::string& str, std::int64_t* intval) {
    try {
        *intval = std::stoll(str);
    } catch (...) {
        fprintf(stderr, "Failed to parse '%s' to int64_t\n", str.c_str());
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

static void _copyToStrBuf(char dst[], size_t dst_size, char* src) {
    memset(dst, 0, dst_size);
    strncpy(dst, src, dst_size - 1);
}

#define copyToStrBuf(dst, argv) _copyToStrBuf(dst, sizeof(dst), argv)

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
                        TgBotCommandData::WriteMsgToChatId data{};
                        if (!stol_or(argv[0], &data.to)) {
                            break;
                        }
                        copyToStrBuf(data.msg, argv[1]);
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
                        TgBotCommandData::ObserveChatId data{};
                        ret = stol_or(argv[0], &data.id) && stob_or(argv[1], &data.observe);
                        data_g.data_4 = data;
                    } break;
                    case CMD_SEND_FILE_TO_CHAT_ID: {
                        TgBotCommandData::SendFileToChatId data{};
                        ret = stol_or(argv[0], &data.id) && parseOneEnum(&data.type, TYPE_MAX,
                                                                         argv[1], "type");
                        copyToStrBuf(data.filepath, argv[2]);
                        data_g.data_5 = data;
                    } break;
                    case CMD_OBSERVE_ALL_CHATS: {
                        TgBotCommandData::ObserveAllChats data = false;
                        ret = stob_or(argv[0], &data);
                        data_g.data_6 = data;
                    } break;
                    case CMD_MAX:
                        break;
                    default:
                        throw runtime_errorf("Unhandled command value: %d!", cmd);
                };
                if (!ret)
                    fprintf(stderr, "Failed parsing arguments for %s\n", TgBotCmd_toStr(cmd).c_str());
            }
        }
    }
    if (ret)
        writeToSocket({cmd, data_g});
    else
        usage(exe, false);
    return !ret;
}
