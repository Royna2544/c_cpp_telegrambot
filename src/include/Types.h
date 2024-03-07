#pragma once

#include <cstdint>

using UserId = std::int64_t;
using ChatId = std::int64_t;
using MessageId = std::int32_t;

#ifndef __WIN32
#include "internal/_FileDescriptor_posix.h"
#endif
