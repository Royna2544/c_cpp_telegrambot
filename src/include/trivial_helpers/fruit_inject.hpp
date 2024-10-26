#pragma once

#define APPLE_INJECT(ctor) using Inject = ctor; ctor
#define APPLE_EXPLICIT_INJECT(ctor) using Inject = ctor; explicit ctor