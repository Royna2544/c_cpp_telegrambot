#include <CStringLifetime.h>

#include <InstanceClassBase.hpp>
#include <initcalls/Initcall.hpp>
#include "StringResLoader.hpp"

#if __has_include("resources.gen.h")
#include "resources.gen.h"
#endif

// Shorthand macro for getting a string
#define GETSTR(x) StringResManager::getInstance()->getString(STRINGRES_##x)
#define GETSTR_IS(x) (GETSTR(x) + ": ")
#define GETSTR_BRACE(x) ("(" + GETSTR(x) + ")")

struct StringResManager : InstanceClassBase<StringResManager>, InitCall, StringResLoader {
    void doInitCall() override;
    const CStringLifetime getInitCallName() const override;
};