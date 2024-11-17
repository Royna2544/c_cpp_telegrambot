#pragma once

#include <Random.hpp>
#include <gmock/gmock.h>

class MockRandom : public RandomBase {
   public:
    using ret_type = Random::ret_type;
    APPLE_INJECT(MockRandom()) = default;

    MOCK_METHOD(ret_type, generate, (const ret_type min, const ret_type max),
                (const, override));
    MOCK_METHOD(void, shuffle, (std::vector<std::string>&), (const, override));
};