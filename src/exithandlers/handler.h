#pragma once

using exit_handler_t = void(*)(int);

void installExitHandler(exit_handler_t fn);
