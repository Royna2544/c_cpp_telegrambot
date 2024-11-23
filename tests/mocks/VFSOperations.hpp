#pragma once

#include <bot/TgBotSocketFileHelperNew.hpp>
#include <gmock/gmock.h>

class VFSOperationsMock : public VFSOperations {
   public:
    APPLE_INJECT(VFSOperationsMock()) = default;

    MOCK_METHOD(bool, writeFile,
                (const std::filesystem::path& filename,
                 const uint8_t* startAddr, size_t size),
                (override));

    MOCK_METHOD(std::optional<SharedMalloc>, readFile,
                (const std::filesystem::path& filename), (override));

    MOCK_METHOD(bool, exists, (const std::filesystem::path& path), (override));

    MOCK_METHOD(void, SHA256, (const SharedMalloc& memory, HashContainer& data),
                (override));
};