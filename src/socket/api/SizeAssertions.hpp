#pragma once

#include "Callbacks.hpp"
#include "DataStructures.hpp"
#include "Packet.hpp"

namespace TgBotSocket {

/**
 * @brief Compile-time size validation for binary compatibility
 * 
 * These assertions ensure struct sizes remain stable across
 * compiler versions and platforms for wire protocol compatibility.
 */
namespace SizeValidation {

// Helper macro for readable assertions
#define ASSERT_SIZE(DataType, DataSize)           \
    static_assert(sizeof(DataType) == (DataSize), \
                  "Size of " #DataType " has changed")

// Packet structures
ASSERT_SIZE(Packet::Header, 80);

// Data structures
ASSERT_SIZE(data::WriteMsgToChatId, 264);
ASSERT_SIZE(data::ObserveChatId, 16);
ASSERT_SIZE(data::SendFileToChatId, 272);
ASSERT_SIZE(data::ObserveAllChats, 8);
ASSERT_SIZE(data::FileTransferMeta, 552);

// Chunked transfer structures
ASSERT_SIZE(data::FileTransferBegin, 304);
ASSERT_SIZE(data::FileTransferChunk, 16);
ASSERT_SIZE(data::FileTransferChunkResponse, 264);
ASSERT_SIZE(data::FileTransferEnd, 8);

// Callback structures
ASSERT_SIZE(callback::GetUptimeCallback, 24);
ASSERT_SIZE(callback::GenericAck, 264);

#undef ASSERT_SIZE

}  // namespace SizeValidation

}  // namespace TgBotSocket