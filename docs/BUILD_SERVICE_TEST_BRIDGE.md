# Build Service Test Bridge

## Overview

This document describes the test bridge pattern implemented for the build services in the Glider Telegram bot. The bridge provides a testable abstraction layer that allows the build service implementations to be mocked and tested independently.

## Problem Statement

The original implementation had the `KernelBuildHandler` class directly depend on gRPC stubs:

```cpp
std::unique_ptr<LinuxKernelBuildService::Stub> stub;
```

This tight coupling made it difficult to:
1. Unit test the handler without spinning up a real gRPC server
2. Test error scenarios and edge cases
3. Mock out the build service for isolated testing

## Solution

We introduced an abstraction layer through interfaces and dependency injection:

### 1. Abstract Interface (`ILinuxKernelBuildService`)

The interface defines all the operations that can be performed on the build service:

- `updateConfig()` - Update kernel configuration
- `prepareBuild()` - Prepare a build (make defconfig)
- `doBuild()` - Execute the actual build
- `cancelBuild()` - Cancel an ongoing build
- `getArtifact()` - Retrieve the build artifact

Key characteristics:
- Uses callbacks for streaming operations (prepareBuild, doBuild, getArtifact)
- Returns boolean to indicate success/failure
- Platform-agnostic (not tied to gRPC)

### 2. Concrete gRPC Implementation (`GrpcLinuxKernelBuildService`)

This class wraps the generated gRPC stub and implements the interface:

- Translates interface calls to gRPC calls
- Handles streaming via callbacks
- Provides two constructors: one accepting a channel, one accepting a server address

### 3. Mock Implementation (`MockLinuxKernelBuildService`)

Uses Google Mock to provide a testable implementation:

- All methods are `MOCK_METHOD`s
- Can be configured with `EXPECT_CALL` in tests
- Allows simulation of various scenarios (success, failure, streaming data)

### 4. Refactored Handler

The `KernelBuildHandler` was refactored to:

1. Accept the interface via dependency injection:
```cpp
std::shared_ptr<tgbot::builder::linuxkernel::ILinuxKernelBuildService> buildService;
```

2. Provide two constructors:
   - Production constructor: creates `GrpcLinuxKernelBuildService`
   - Test constructor: accepts injected implementation

3. Use the interface instead of direct gRPC stub calls

## Usage

### Production Code

```cpp
// Original constructor still works - creates gRPC implementation internally
auto handler = std::make_shared<KernelBuildHandler>(
    api, commandLine, auth, configManager);
```

### Test Code

```cpp
// Create mock service
auto mockService = std::make_shared<MockLinuxKernelBuildService>();

// Configure expectations
EXPECT_CALL(*mockService, prepareBuild(_, _))
    .WillOnce(Invoke([](const BuildPrepareRequest& req,
                        std::function<void(const BuildStatus&)> callback) {
        BuildStatus status;
        status.set_status(ProgressStatus::SUCCESS);
        status.set_build_id(42);
        callback(status);
        return true;
    }));

// Inject mock into handler
auto handler = std::make_shared<KernelBuildHandler>(
    api, commandLine, auth, configManager,
    mockService, systemMonitorStub, healthStub);

// Test handler behavior without real gRPC server
```

## Benefits

1. **Testability**: Build handlers can be tested without real build services
2. **Isolation**: Tests run faster and don't depend on external services
3. **Flexibility**: Easy to simulate various scenarios (errors, edge cases, timeouts)
4. **Maintainability**: Changes to gRPC implementation don't affect tests
5. **Documentation**: Interface serves as a clear contract

## Files Added

- `src/api/builtin_modules/builder/kernel/ILinuxKernelBuildService.hpp` - Abstract interface
- `src/api/builtin_modules/builder/kernel/GrpcLinuxKernelBuildService.hpp` - gRPC implementation
- `tests/mocks/LinuxKernelBuildService.hpp` - Mock implementation
- `tests/LinuxKernelBuildServiceTest.cpp` - Unit tests demonstrating usage

## Files Modified

- `src/api/builtin_modules/builder/kernel/kernelbuild.cpp` - Refactored to use interface
- `tests/CMakeLists.txt` - Added new test target

## Future Work

The same pattern can be applied to other build services:
- ROM Build Service
- System Monitor Service
- Health Check Service

This would provide comprehensive testability across all build-related functionality.

## Testing

Run the tests with:

```bash
cd build
ctest --output-on-failure --timeout 30 -R kernelbuildservice
```

Or build and run specific test:

```bash
cmake --build build --target test_kernelbuildservice
./build/bin/test_kernelbuildservice
```
