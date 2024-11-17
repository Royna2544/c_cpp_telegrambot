#pragma once

#include <gmock/gmock.h>
#include <ResourceManager.h>

class MockResource : public ResourceProvider {
   public:
    APPLE_INJECT(MockResource()) = default;

    MOCK_METHOD(std::string_view, get, (std::filesystem::path filename),
                (const, override));
    MOCK_METHOD(bool, preload, (std::filesystem::path p), ());
};