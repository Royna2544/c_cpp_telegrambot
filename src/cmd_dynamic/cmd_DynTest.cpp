#include "cmd_dynamic.h"

static void cmdTest(const Bot& bot, const Message::Ptr& message) {}

DECL_DYN_COMMAND(true, "test", cmdTest);
