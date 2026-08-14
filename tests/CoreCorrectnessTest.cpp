#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <limits>
#include <stop_token>
#include <string>

#include "GlobalStorage.hpp"
#include "SharedMalloc.hpp"
#include "imagep/ImagePBase.hpp"
#include "imagep/MediaLimits.hpp"
#include "imagep/WebPValidation.hpp"
#include "ml/Csv.hpp"

TEST(SharedMallocTest, RejectsNegativeAndOverflowingRanges) {
    SharedMalloc memory(8);
    std::array<std::byte, 8> bytes{};

    EXPECT_THROW(memory.assignFrom(bytes.data(), 1, -9), std::out_of_range);
    EXPECT_THROW(memory.assignTo(bytes.data(), 1, -9), std::out_of_range);
    EXPECT_THROW(memory.assignFrom(bytes.data(),
                                   std::numeric_limits<std::size_t>::max(), 1),
                 std::out_of_range);
    EXPECT_THROW(memory.move(-9, 0, 1), std::out_of_range);
    EXPECT_THROW(memory.move(7, 0, 2), std::out_of_range);
    EXPECT_THROW(memory.move(0, 7, 2), std::out_of_range);
}

TEST(SharedMallocTest, ResizeMayReportAllocationFailure) {
    static_assert(!noexcept(std::declval<const SharedMalloc&>().resize(1)));
}

TEST(GlobalStorageTest, ReturnedValueKeepsAllocationAliveAfterRemoval) {
    GlobalStorage storage;
    auto value = storage.getStorage("key");
    value.resize(sizeof(int));
    value.assignFrom(42);

    storage.removeStorage("key");

    int result = 0;
    value.assignTo(result);
    EXPECT_EQ(result, 42);
}

TEST(WebPValidationTest, ChecksDecodedByteLimitWithoutOverflow) {
    using imagep::webp::decodedByteSize;
    constexpr std::size_t limit = 100U * 1024U * 1024U;

    EXPECT_EQ(decodedByteSize(1, 1, limit), 4U);
    EXPECT_EQ(decodedByteSize(5000, 5000, limit), 100000000U);
    EXPECT_FALSE(decodedByteSize(50000, 50000, limit).has_value());
    EXPECT_FALSE(decodedByteSize(std::numeric_limits<int>::max(),
                                 std::numeric_limits<int>::max(), limit)
                     .has_value());
    EXPECT_FALSE(decodedByteSize(0, 10, limit).has_value());
    EXPECT_FALSE(decodedByteSize(-1, 10, limit).has_value());
}

TEST(MediaLimitsTest, RejectsOverflowingAndOversizedMetadata) {
    using namespace imagep::limits;

    EXPECT_TRUE(imageDimensionsAllowed(4096, 4096));
    EXPECT_FALSE(imageDimensionsAllowed(8193, 1));
    EXPECT_FALSE(imageDimensionsAllowed(4096, 4097));
    EXPECT_FALSE(imageDimensionsAllowed(0, 100));
    EXPECT_FALSE(
        imageDimensionsAllowed(std::numeric_limits<std::int64_t>::max(),
                               std::numeric_limits<std::int64_t>::max()));
    EXPECT_TRUE(compressedSizeAllowed(kMaxCompressedInputBytes));
    EXPECT_FALSE(compressedSizeAllowed(kMaxCompressedInputBytes + 1));
    EXPECT_FALSE(compressedSizeAllowed(-1));
}

TEST(MediaLimitsTest, ProcessingControlHonorsCancellationAndDeadline) {
    std::stop_source source;
    PhotoBase::ProcessingControl cancelled{source.get_token()};
    EXPECT_FALSE(cancelled.shouldStop());
    source.request_stop();
    EXPECT_TRUE(cancelled.shouldStop());

    PhotoBase::ProcessingControl expired{
        {}, std::chrono::steady_clock::now() - std::chrono::milliseconds(1)};
    EXPECT_TRUE(expired.shouldStop());
}

TEST(RangeRestrictedTest, NormalizesBothDirectionsWithoutSignedOverflow) {
    EXPECT_EQ(static_cast<int>(RangeRestricted<0, 360>(360)), 0);
    EXPECT_EQ(static_cast<int>(RangeRestricted<0, 360>(450)), 90);
    EXPECT_EQ(static_cast<int>(RangeRestricted<0, 360>(-90)), 270);
    EXPECT_GE(static_cast<int>(
                  RangeRestricted<0, 360>(std::numeric_limits<int>::min())),
              0);
}

TEST(CsvTest, EscapesAndParsesQuotesCommasAndNewlines) {
    const std::vector<std::string> fields = {
        "plain", "with,comma", "with \"quote\"", "two\nlines", ""};
    const auto encoded = csv::encodeRow(fields);
    EXPECT_EQ(
        encoded,
        "plain,\"with,comma\",\"with \"\"quote\"\"\",\"two\nlines\",\"\"\n");
    EXPECT_EQ(csv::parseRow(encoded), fields);
}

TEST(CsvTest, RejectsMalformedQuotedRows) {
    EXPECT_FALSE(csv::tryParseRow("\"unterminated", nullptr));
    EXPECT_FALSE(csv::tryParseRow("\"field\"junk", nullptr));
}
