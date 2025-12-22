#pragma once

/**
 * @file ApiDef.hpp
 * @brief Main API header for TgBot Socket Protocol
 * 
 * This header provides backward compatibility by including all
 * API components. For new code, prefer including specific headers.
 */

// Core protocol definitions
#include "api/CoreTypes.hpp"
#include "api/Commands.hpp"
#include "api/ByteHelper.hpp"
#include "api/Utilities.hpp"

// Protocol structures
#include "api/Packet.hpp"
#include "api/DataStructures.hpp"
#include "api/Callbacks.hpp"

// Size validation
#include "api/SizeAssertions.hpp"

// Re-export commonly used types for convenience
namespace TgBotSocket {

// Version information
using Protocol::MAGIC_VALUE;
using Protocol::MAGIC_VALUE_BASE;
using Protocol::DATA_VERSION;

// Limits
using Limits::MAX_PATH_SIZE;
using Limits::MAX_MSG_SIZE;
using Limits::ALIGNMENT;

}  // namespace TgBotSocket