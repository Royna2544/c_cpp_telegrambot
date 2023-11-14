#include "handler.h"

exit_handler_t exitHandlerInt;
exit_handler_v_t exitHandlerVoid;

static void exitHandlerVoidDef(void) {
    exitHandlerInt(0);
}

void installExitHandler(exit_handler_t fn) {
    exitHandlerInt = fn;
    exitHandlerVoid = exitHandlerVoidDef;
    _installExitHandler();
}
