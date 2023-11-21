#pragma once

#include <cstdint>
#include <cinttypes>

// Helper for platform-independent 64bit int format
#define LONGFMT "%" PRId64

using UserId = std::int64_t;
using ChatId = std::int64_t;
using MessageId = std::int32_t;
