#pragma once

#ifdef __WIN32
#include "SelectorWindows.hpp"
using DefaultSelector = SelectSelector;
#else   // __WIN32
#include "SelectorPosix.hpp"
using DefaultSelector = PollSelector;
#endif  // __WIN32