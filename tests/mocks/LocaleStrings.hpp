
#pragma once

#include <StringResLoader.hpp>
#include <gmock/gmock.h>

struct MockLocaleStrings : public StringResLoaderBase::LocaleStrings {
   public:
    APPLE_INJECT(MockLocaleStrings()) = default;

    MOCK_METHOD(std::string_view, get, (const Strings& string),
                (const, override));
    MOCK_METHOD(size_t, size, (), (override, const, noexcept));
};
