
#pragma once

#include <api/StringResLoader.hpp>
#include <gmock/gmock.h>

struct MockLocaleStrings : public StringResLoader::PerLocaleMap {
   public:
    APPLE_INJECT(MockLocaleStrings()) = default;

    MOCK_METHOD(std::string_view, get, (const Strings string),
                (const, override));
};
